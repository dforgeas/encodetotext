#include "encodetotext.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <experimental/string_view>

using namespace std;
using string_view = experimental::string_view;

static bool ends_with(const string_view str, const string_view end)
{
   return end.size() <= str.size()
      && end == str.substr(str.size() - end.size());
}

static int process(int argc, char *argv[])
{
   string_view mode;
   ifstream in;
   ofstream out;
   if (argc > 3)
   {
      mode = argv[1];
      if (mode != "enc" && mode != "dec")
      {
         cerr << "invalid mode " << mode << " ; valid is enc or dec\n";
         return 2;
      }
      if (ends_with(argv[3], "words.txt"))
      { // basic protection of not overwriting our database
         cerr << "cannot use words.txt as filename\n";
         return 4;
      }
      in.open(argv[2], ios::in | ios::binary);
      out.open(argv[3], ios::out | ios::binary);
      if (!in)
      {
         cerr << "error opening " << argv[2] << '\n';
         return 3;
      }
      if (!out)
      {
         cerr << "error opening " << argv[3] << '\n';
         return 4;
      }
   }
   else
   {
      cerr << "missing parameters: mode {enc, dec}, filename_in, filename_out\n";
      return 1;
   }

   vector<string> words;
   if ( ! quick_start(words))
   {
      std::clock_t startTime(std::clock());

      generate_words(words);

      std::clock_t duration(std::clock() - startTime);
      std::cerr << "generate_words in " << (float(duration) / CLOCKS_PER_SEC) << "s." << endl;

      save_words(words);
   }
   load_static_key();

   if (mode == "enc")
   {
      cerr << "encoding the file..." << endl;
      encode(words, in, out);
   }
   else
   {
      unordered_map<string, uint16_t> words_rev;
      reverse_words(words, words_rev);

      cerr << "decoding the file..." << endl;
      decode(words_rev, in, out);
   }

   return 0;
}

int (*run)(int argc, char *argv[]) = process;
