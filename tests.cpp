#include "encodetotext.hpp"

#include <fstream>
#include <iostream>
#include <cassert>

using namespace std;

#if FULL_TESTS
static void writeToFile(istream& data, const string& filename)
{
   ofstream out(filename.c_str(), std::ios::binary);
   assert(out.is_open());
   out << data.rdbuf(); // this just copies everything
}
#endif

static int unit_tests(int argc, char *argv[])
{
   streamsize start, stop;
   if (argc > 2)
   {
      {
         istringstream read_start(argv[1]);
         read_start >> start;
         if (read_start.fail())
         {
            cerr << "parameter start must be a number\n";
            return 2;
         }
      }

      {
         istringstream read_stop(argv[2]);
         read_stop >> stop;
         if (read_stop.fail())
         {
            cerr << "parameter stop must be a number\n";
            return 3;
         }
      }

      if (start >= stop)
      {
         cerr << "start must be less than stop\n";
         return 4;
      }
   }
   else
   {
      cerr << "missing parameters: start, stop\n";
      return 1;
   }

   vector<small_string> words;
   if ( ! quick_start(words))
   {
      generate_words(words);
      save_words(words);
   }
   load_static_key();

   cerr << "starting tests..."<< endl;
   vector<bool> results(stop - start);
   int progress = 0;
   const int max_progress = 80;

   for (streamsize n = start; n < stop; ++n)
   { // test for size n
      stringstream in, out, result;
      for (streamsize i = 0; i < n; ++i)
      {
         in << static_cast<char>(i + 'a');
      }

      try
      {
         /* prepare and encode */
         in.clear();
         in.seekg(0);
         assert(0 == in.tellg());
         assert(0 == out.tellp());
         encode(words, in, out);

#if FULL_TESTS
         /* save the encoded data */
         ostringstream filename;
         filename << "data/out" << n << ".txt";
         writeToFile(out, filename.str());
#endif

         /* prepare and decode */
         out.clear();
         out.seekg(0);
         assert(0 == out.tellg());
         assert(0 == result.tellp());
         decode(words, out, result);
      }
      catch (const exception& exc)
      {
        cerr << "error (#" << n << "): " << exc.what() << endl;
      } // catch any exception but allow other tests to run

      results[n - start] = in.str() == result.str();

      /* display a progress bar */
      const int new_progress = (n - start) * max_progress / (stop - start - 1);
      if (new_progress != progress)
      {
         progress = new_progress;
         cerr.put('\r');
         for (int i = 0; i < new_progress; ++i) cerr.put('#');
         for (int i = new_progress; i < max_progress; ++i) cerr.put('.');
      }
   }

   /* display the results */
   streamsize total = 0, failed = 0;
   cout << "\nFAILED: ";
   for (vector<bool>::const_iterator it = results.begin()
       ; it != results.end()
       ; ++it)
   {
      ++total;
      if ( ! *it)
      {
         ++failed;
         cout << (start + (it - results.begin())) << ' ';
      }
   }
   cout << '\n';
   cout << "failed " << (failed * 100 / total) << "%\n";
   return 0;
}

int (*run)(int argc, char *argv[]) = unit_tests;
