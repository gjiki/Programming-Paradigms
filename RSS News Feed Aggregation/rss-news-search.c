#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "vector.h"
#include "hashset.h"
#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

static const char * const kWelcomeTextFile = "data/welcome.txt";
static const char * const filePrefix = "file://";
static const char * const kDefaultFeedsFile = "data/test.txt";
static const char * const kDefaultStopWordsFile = "data/stop-words.txt";

static const signed long kHashMultiplier = -1664117991L;
static const int kStopWordsNumBuckets = 1009;

typedef struct
{
	const char * title;
	const char * server_name;
	const char * url_name;
	int cnt;
} article;

typedef struct
{
    const char * word;
    vector * feeds;
} rss_word;

static void Welcome(const char * welcomeTextFileName);
static void BuildIndices(const char * feedsFileName, hashset * stop_words, hashset * rss_words_hashset);
static void ProcessFeed(const char * remoteDocumentName, hashset * stop_words, hashset * rss_words_hashset);
static void ProcessFeedFromFile(char * fileName, hashset * stop_words, hashset * rss_words_hashset);

static void PullAllNewsItems(urlconnection * urlconn, hashset * stop_words, hashset * rss_words_hashset);
static bool GetNextItemTag(streamtokenizer * st);
static void ProcessSingleNewsItem(streamtokenizer * st, hashset * stop_words, hashset * rss_words_hashset);
static void ExtractElement(streamtokenizer * st, const char * htmlTag, char dataBuffer[], int bufferLength);

static void ParseArticle(const char  * articleTitle, const char * articleDescription, const char * articleURL,
    hashset * stop_words, hashset * rss_words_hashset);
static void ScanArticle(streamtokenizer * st, const char * articleTitle, const char * unused, const char * articleURL,
    hashset * stop_words, hashset * rss_words_hashset);

static void QueryIndices(hashset * stop_words, hashset * rss_words_hashset);
static void ProcessResponse(const char * word, hashset * stop_words, hashset * rss_words_hashset);
static bool WordIsWellFormed(const char * word);

int stop_word_cmp_fn(const void * str1, const void * str2);
static int stop_word_hash_fn(const void * void_s, int num_buckets) ;
void stop_word_free_fn(void * word);

static int rss_word_hash_fn(const void * rss_word_ptr, int num_buckets);
int rss_word_cmp_fn(const void * rss_word1, const void * rss_word2);
void rss_word_free_fn(void * rss_word_ptr);

int article_cmp_fn(const void * art1, const void * art2);
void article_free_fn(void * art_ptr);

void load_stop_words(hashset * stop_words, const char * kStopWordsFile);
static int vector_sort_fn(const void * var1, const void * var2);
void print_result(vector * articles);

static const char * const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";

/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to
 * map words to the collection of news articles where that
 * word appears.
 */

int main(int argc, char ** argv)
{
	hashset stop_words;
	HashSetNew(&stop_words, sizeof(char **), kStopWordsNumBuckets, stop_word_hash_fn, stop_word_cmp_fn, stop_word_free_fn);

	hashset rss_words_hashset;
	HashSetNew(&rss_words_hashset, sizeof(rss_word), kStopWordsNumBuckets, rss_word_hash_fn, rss_word_cmp_fn, rss_word_free_fn);

	setbuf(stdout, NULL);
	Welcome(kWelcomeTextFile);
	load_stop_words(&stop_words, kDefaultStopWordsFile);

	BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1], &stop_words, &rss_words_hashset);
	QueryIndices(&stop_words, &rss_words_hashset);

	HashSetDispose(&stop_words);
	HashSetDispose(&rss_words_hashset);
}


/**
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */

static const char * const kNewLineDelimiters = "\r\n";
static void Welcome(const char * welcomeTextFileName)
{
	FILE * infile;
	streamtokenizer st;
	char buffer[1024];

	infile = fopen(welcomeTextFileName, "r");
	assert(infile != NULL);

	STNew(&st, infile, kNewLineDelimiters, true);
	while (STNextToken(&st, buffer, sizeof(buffer)))
	{
		printf("%s\n", buffer);
	}

	printf("\n");
	STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one..
	fclose(infile);
}

void load_stop_words(hashset * stop_words, const char * kStopWordsFile)
{
	FILE * infile;

 	streamtokenizer st;
 	char buffer[1024];

 	infile = fopen(kStopWordsFile, "r");
 	assert(infile != NULL);

 	STNew(&st, infile, kNewLineDelimiters, true);
	while (STNextToken(&st, buffer, sizeof(buffer)))
	{
		char * new_word = strdup(buffer);
		HashSetEnter(stop_words, &new_word);
	}

	STDispose(&st);
	fclose(infile);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 */

static void BuildIndices(const char * feedsFileName, hashset * stop_words, hashset * rss_words_hashset)
{
	FILE * infile;
	streamtokenizer st;
	char remoteFileName[1024];

	infile = fopen(feedsFileName, "r");
	assert(infile != NULL);
	STNew(&st, infile, kNewLineDelimiters, true);

	while (STSkipUntil(&st, ":") != EOF)
	{
		// ignore everything up to the first selicolon of the line
		STSkipOver(&st, ": ");		 // now ignore the semicolon and any whitespace directly after it
		STNextToken(&st, remoteFileName, sizeof(remoteFileName));
		ProcessFeed(remoteFileName, stop_words, rss_words_hashset);
	}

	STDispose(&st);
	fclose(infile);
	printf("\n");
}


/**
 * Function: ProcessFeedFromFile
 * ---------------------
 * ProcessFeed locates the specified RSS document, from locally
 */

static void ProcessFeedFromFile(char * fileName, hashset * stop_words, hashset * rss_words_hashset)
{
	FILE * infile;
	streamtokenizer st;
	char articleDescription[1024];

	articleDescription[0] = '\0';
	infile = fopen((const char *)fileName, "r");
	assert(infile != NULL);

	STNew(&st, infile, kTextDelimiters, true);
	ScanArticle(&st, (const char *)fileName, articleDescription, (const char *)fileName, stop_words, rss_words_hashset);

	STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one..
	fclose(infile);
}

/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 */

static void ProcessFeed(const char * remoteDocumentName, hashset * stop_words, hashset * rss_words_hashset)
{
	if (!strncmp(filePrefix, remoteDocumentName, strlen(filePrefix)))
	{
		ProcessFeedFromFile((char *)remoteDocumentName + strlen(filePrefix), stop_words, rss_words_hashset);
		return;
	}

	url u;
	urlconnection urlconn;

	URLNewAbsolute(&u, remoteDocumentName);
	URLConnectionNew(&urlconn, &u);

	switch (urlconn.responseCode)
	{
	  case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.server_name);
		      break;
	  case 200: PullAllNewsItems(&urlconn, stop_words, rss_words_hashset);
		        break;
	  case 301:
	  case 302: ProcessFeed(urlconn.newUrl, stop_words, rss_words_hashset);
		        break;
	  default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
			  u.server_name, u.file_name, urlconn.responseCode, urlconn.responseMessage);
		   break;
	};

	URLConnectionDispose(&urlconn);
	URLDispose(&u);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * PullAllNewsItems reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ProcessSingleNewsItem, which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. PullAllNewsItems processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ProcessSingleNewsItem do process everything up until </item>.
 */

static void PullAllNewsItems(urlconnection * urlconn, hashset * stop_words, hashset * rss_words_hashset)
{
	streamtokenizer st;
	STNew(&st, urlconn->dataStream, kTextDelimiters, false);

	while (GetNextItemTag(&st))
	{
		// if true is returned, then assume that <item ...> has just been read and pulled from the data stream
		ProcessSingleNewsItem(&st, stop_words, rss_words_hashset);
	}

	STDispose(&st);
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.
 *
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 */

static const char * const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer * st)
{
	char htmlTag[1024];
	while (GetNextTag(st, htmlTag, sizeof(htmlTag)))
	{
		if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0)
		{
	  		return true;
		}
	}
	return false;
}

/**
 * Function: ProcessSingleNewsItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML feed.
 * At the moment this function is called, we're to assume that the <item> tag was just
 * read and that the streamtokenizer is currently pointing to everything else, as with:
 *
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>, storing the title, link, and article
 * description in local buffers long enough so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in any particular order.  We do asssume that
 * the link field exists (although we can certainly proceed if the title and article descrption are missing.)  There
 * are often other tags inside an item, but we ignore them.
 */

static const char * const kItemEndTag = "</item>";
static const char * const kTitleTagPrefix = "<title";
static const char * const kDescriptionTagPrefix = "<description";
static const char * const kLinkTagPrefix = "<link";
static void ProcessSingleNewsItem(streamtokenizer * st, hashset * stop_words, hashset * rss_words_hashset)
{
	char htmlTag[1024];
	char articleTitle[1024];
	char articleDescription[1024];
	char articleURL[1024];
	articleTitle[0] = articleDescription[0] = articleURL[0] = '\0';

	while (GetNextTag(st, htmlTag, sizeof(htmlTag)) && (strcasecmp(htmlTag, kItemEndTag) != 0))
	{
		if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0)
			ExtractElement(st, htmlTag, articleTitle, sizeof(articleTitle));
		if (strncasecmp(htmlTag, kDescriptionTagPrefix, strlen(kDescriptionTagPrefix)) == 0)
			ExtractElement(st, htmlTag, articleDescription, sizeof(articleDescription));
		if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0)
			ExtractElement(st, htmlTag, articleURL, sizeof(articleURL));
	}

	if (strncmp(articleURL, "", sizeof(articleURL)) == 0) return;     // punt, since it's not going to take us anywhere
	ParseArticle(articleTitle, articleDescription, articleURL, stop_words, rss_words_hashset);
}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching end tag.  It assumes that
 * the most recently extracted HTML tag resides in the buffer addressed by htmlTag.  The implementation
 * populates the specified data buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.  Assuming for illustration purposes
 * that htmlTag addresses a buffer containing "<description" followed by other text, these three scanarios are
 * handled:
 *
 *    Normal Situation:     <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.  This is not uncommon
 * for the description data to be missing, so we need to cover all three scenarious (I've actually seen all three.)
 * It would be quite unusual for the title and/or link fields to be empty, but this handles those possibilities too.
 */

static void ExtractElement(streamtokenizer * st, const char * htmlTag, char dataBuffer[], int bufferLength)
{
	assert(htmlTag[strlen(htmlTag) - 1] == '>');
	if (htmlTag[strlen(htmlTag) - 2] == '/') return;    // e.g. <description/> would state that a description is not being supplied

	STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "<");
	RemoveEscapeCharacters(dataBuffer);

	if (dataBuffer[0] == '<') strcpy(dataBuffer, "");  // e.g. <description></description> also means there's no description
	STSkipUntil(st, ">");
	STSkipOver(st, ">");
}

/**
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the three
 * parameters.  The network connection is either established of not.  The implementation
 * is prepared to handle a subset of possible (but by far the most common) scenarios,
 * and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be contacted.
 *    200 means that the document exists and that a connection to that very document has
 *        been established.
 *    301 means that the document has moved to a new location
 *    302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist (404),
 *        or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them, since
 * no others appears all that often, and it'd be tedious to be fully exhaustive in our
 * enumeration of all possibilities.
 */

static void ParseArticle(const char * articleTitle, const char * articleDescription, const char * articleURL,
    hashset * stop_words, hashset * rss_words_hashset)
{
	url u;
	urlconnection urlconn;
	streamtokenizer st;

	URLNewAbsolute(&u, articleURL);
	URLConnectionNew(&urlconn, &u);

	switch (urlconn.responseCode)
	{
		case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
				break;
	  	case 200: printf("Scanning \"%s\" from \"http://%s\"\n", articleTitle, u.server_name);
		    	STNew(&st, urlconn.dataStream, kTextDelimiters, false);
				ScanArticle(&st, articleTitle, articleDescription, articleURL, stop_words, rss_words_hashset);
				STDispose(&st);
				break;
		case 301:
		case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
				ParseArticle(articleTitle, articleDescription, urlconn.newUrl, stop_words, rss_words_hashset);
				break;
		default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.server_name, urlconn.responseCode);
				break;
	}

	URLConnectionDispose(&urlconn);
	URLDispose(&u);
}

/**
 * Function: ScanArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 */

static void ScanArticle(streamtokenizer * st, const char * articleTitle, const char * unused, const char * articleURL,
    hashset * stop_words, hashset * rss_words_hashset)
{
	int numWords = 0;
	char word[1024];
	char longestWord[1024] = {'\0'};

	while (STNextToken(st, word, sizeof(word)))
	{
		if (strcasecmp(word, "<") == 0)
		{
			SkipIrrelevantContent(st); // in html-utls.h
		} else {
			RemoveEscapeCharacters(word);
			char * word_ptr = strdup(word);

			if (WordIsWellFormed(word) && HashSetLookup(stop_words, &word_ptr) == NULL)
			{
				numWords++;

				article * art = malloc(sizeof(article));
				art->title = strdup(articleTitle);
                art->server_name = strdup(unused);
                art->url_name = strdup(articleURL);
                art->cnt = 1;

                rss_word * rss_str = malloc(sizeof(rss_str));
                rss_str->word = word_ptr;
                rss_str->feeds = NULL;

                void * pos_ptr = HashSetLookup(rss_words_hashset, rss_str);

                if (pos_ptr == NULL)
                {
                    vector * vec_ptr = malloc(sizeof(vector));
                    VectorNew(vec_ptr, sizeof(article), article_free_fn, 0);
                    rss_str->feeds = vec_ptr;

                    VectorAppend(vec_ptr, art);
                    HashSetEnter(rss_words_hashset, rss_str);
                } else {
                    rss_word_free_fn(rss_str);
                    rss_word * rss = (rss_word *)pos_ptr;
                    vector * vec = rss->feeds;
                    int index = VectorSearch(vec, art, article_cmp_fn, 0, false);

                    if (index != -1)
                    {
                        article_free_fn(art);
                        article * art_ptr = (article *)VectorNth(vec, index);
                        (art_ptr->cnt)++;
                    } else {
                        VectorAppend(vec, art);
                    }
                }

                free(rss_str);
                free(art);

                if (strlen(word) > strlen(longestWord))
                    strcpy(longestWord, word);
            } else free(word_ptr);
		}
	}

	printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
	printf("\tThe longest word scanned was \"%s\".", longestWord);

	if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL))
		printf(" [Ooooo... long word!]");
	printf("\n");
}

/**
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */

static void QueryIndices(hashset * stop_words, hashset * rss_words_hashset)
{
	char response[1024];
	while (true)
	{
		printf("Please enter a single search term [enter to break]: ");
		fgets(response, sizeof(response), stdin);
		response[strlen(response) - 1] = '\0';

		if (strcasecmp(response, "") == 0) break;
		ProcessResponse(response, stop_words, rss_words_hashset);
	}
}

/**
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char * word, hashset * stop_words, hashset * rss_words_hashset)
{
	if (WordIsWellFormed(word))
	{
        if (HashSetLookup(stop_words, &word) == NULL)
        {
            rss_word rss_to_find;
            rss_to_find.word = word;
            rss_to_find.feeds = NULL;

            rss_word * data = (rss_word *)HashSetLookup(rss_words_hashset, &rss_to_find);

            if (data != NULL)
            {
                vector * res = data->feeds;
                VectorSort(res, vector_sort_fn);
                print_result(res);
            } else printf("None of today's news articles contain the word \"%s\".", word);
        } else {
            printf("Too common a word to be taken seriously. Try something more specific.");
        }
	} else {
		printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
	}
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.
 */

static bool WordIsWellFormed(const char * word)
{
	int i;
	if (strlen(word) == 0) return true;
	if (!isalpha((int) word[0])) return false;

	for (i = 1; i < strlen(word); i++)
		if (!isalnum((int) word[i]) && (word[i] != '-'))
			return false;

	return true;
}

void print_result(vector * articles)
{
    for (int i = 0; i < articles->logicalLength; i++)
    {
        article * curr_art = VectorNth(articles, i);
        const char * title = curr_art->title;

        int cnt = curr_art->cnt;
        const char * URL = curr_art->url_name;

        printf("%d.) ", i + 1);
        printf("\"%s\"", title);

        if (cnt == 1)
        {
            printf(" [search term occurs %d time]\n", cnt);
        } else {
            printf(" [search term occurs %d times]\n", cnt);
        }

        printf("\"%s\"", URL);
        printf("\n");
    }
}

/**
 * StringHash
 * ----------
 * This function adapted from Eric Roberts' "The Art and Science of C"
 * It takes a string and uses it to derive a hash code, which
 * is an integer in the range [0, numBuckets).  The hash code is computed
 * using a method called "linear congruence."  A similar function using this
 * method is described on page 144 of Kernighan and Ritchie.  The choice of
 * the value for the kHashMultiplier can have a significant effect on the
 * performance of the algorithm, but not on its correctness.
 * This hash function has the additional feature of being case-insensitive,
 * hashing "Peter Pawlowski" and "PETER PAWLOWSKI" to the same code.
 */

static int stop_word_hash_fn(const void * void_s, int num_buckets)
{
	int i;
	unsigned long hashcode = 0;
	const char * s = *(const char **)void_s;

	for (i = 0; i < strlen(s); i++)
		hashcode = hashcode * kHashMultiplier + tolower(s[i]);

	return hashcode % num_buckets;
}

int stop_word_cmp_fn(const void * str1, const void * str2)
{
	const char * s1 = *(const char **)str1;
    const char * s2 = *(const char **)str2;
	return strcasecmp(s1, s2);
}

void stop_word_free_fn(void * word)
{
    free(*(void **)word);
}

static int rss_word_hash_fn(const void * rss_word_ptr, int num_buckets)
{
    const rss_word * rss = (const rss_word *)rss_word_ptr;
    return stop_word_hash_fn(&rss->word, num_buckets);
}

int rss_word_cmp_fn(const void * rss_word1, const void * rss_word2)
{
    const rss_word * rss1 = (const rss_word *)rss_word1;
    const rss_word * rss2 = (const rss_word *)rss_word2;
    return strcasecmp(rss1->word, rss2->word);
}

void rss_word_free_fn(void * rss_word_ptr)
{
    rss_word * rss = (rss_word *)rss_word_ptr;
    free((void *)rss->word);

    if (rss->feeds != NULL)
    {
        VectorDispose(rss->feeds);
        free(rss->feeds);
    }
}

int article_cmp_fn(const void * art1, const void * art2)
{
	article * article1 = (article *)art1;
	article * article2 = (article *)art2;

	if (strcasecmp(article1->url_name, article2->url_name) == 0) return 0;
    if (strcasecmp(article1->title, article2->title) == 0 && strcasecmp(article1->server_name, article2->server_name) == 0) return 0;
    return strcasecmp(article1->url_name, article2->url_name);
}

void article_free_fn(void * art_ptr)
{
	article * art = (article *)art_ptr;
	free((void *)art->title);
	free((void *)art->server_name);
	free((void *)art->url_name);
}

static int vector_sort_fn(const void * var1, const void * var2)
{
    const article * first = var1;
    const article * second = var2;

    if (first->cnt > second->cnt)
        return -1;

    if (first->cnt < second->cnt)
        return 1;
    return 0;
}
