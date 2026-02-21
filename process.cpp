#include "encodetotext.hpp"
#include "make_key.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

static bool ends_with(const string_view str, const string_view end)
{
   return end.size() <= str.size()
      && end == str.substr(str.size() - end.size());
}

/**
 * Parses command line arguments and validates them
 *
 * @param argc Argument count
 * @param argv Argument values
 * @param mode Output: processing mode (enc/dec/key)
 * @param input_file Output: input filename or "-" for stdin
 * @param output_file Output: output filename or "-" for stdout
 * @return true if arguments are valid, false otherwise
 * @throws error if invalid arguments are provided
 */
static bool parse_arguments(int argc, char *argv[],
                          string_view& mode,
                          string_view& input_file,
                          string_view& output_file)
{
   if (argc <= 1)
   {
      cerr << "missing arguments: mode {enc, dec, key}, filename_in(or -) or password, [filename_out(or -)]\n";
      return false;
   }

   mode = argv[1];
   if (mode != "enc" && mode != "dec" && mode != "key")
   {
      cerr << "invalid mode " << mode << " ; valid is enc, dec or key\n";
      return false;
   }

   if (mode == "key")
   {
      // Key mode only needs password argument
      if (argc <= 2)
      {
         cerr << "missing arguments: mode {key}, password\n";
         return false;
      }
      // For key mode, we don't need input/output files
      return true;
   }

   // For enc/dec modes, we need input and output files
   if (argc <= 3)
   {
      cerr << "missing arguments: mode {enc, dec}, filename_in(or -), filename_out(or -)\n";
      return false;
   }

   input_file = argv[2];
   output_file = argv[3];

   // Basic protection of not overwriting our database
   if (ends_with(output_file, "words.txt"))
   {
      cerr << "cannot use words.txt as filename\n";
      return false;
   }

   return true;
}

/**
 * Sets up input and output streams based on filenames
 *
 * @param input_file Input filename or "-" for stdin
 * @param output_file Output filename or "-" for stdout
 * @param file_in Output: input file stream (if file used)
 * @param file_out Output: output file stream (if file used)
 * @param in Output: pointer to input stream
 * @param out Output: pointer to output stream
 * @return true if streams were set up successfully, false otherwise
 */
static bool setup_io_streams(const string_view input_file,
                            const string_view output_file,
                            ifstream& file_in, ofstream& file_out,
                            istream*& in, ostream*& out)
{
   // Set up input stream
   if (input_file != "-")
   {
      file_in.open(input_file.data(), ios::binary);
      in = &file_in;
   }
   else
   {
      in = &cin;
   }

   if (!*in)
   {
      cerr << "error opening " << input_file << '\n';
      return false;
   }

   // Set up output stream
   if (output_file != "-")
   {
      file_out.open(output_file.data(), ios::binary);
      out = &file_out;
   }
   else
   {
      out = &cout;
   }

   if (!*out)
   {
      cerr << "error opening " << output_file << '\n';
      return false;
   }

   return true;
}

/**
 * Sets up the word list for encoding/decoding
 *
 * @return vector of small_string words for encoding/decoding
 */
static vector<small_string> setup_word_list()
{
   vector<small_string> words;
   if (!quick_start(words))
   {
      std::clock_t startTime(std::clock());

      generate_words(words);

      std::clock_t duration(std::clock() - startTime);
      std::cerr << "generate_words in " << (float(duration) / CLOCKS_PER_SEC) << "s." << endl;

      save_words(words);
   }
   return words;
}

/**
 * Handles the key generation mode
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
static int handle_key_mode(int argc, char *argv[])
{
   if (argc > 3)
   {
      cerr << "warning: only the first argument is part of the password\n";
   }
   return make_key(argv[2]), 0;
}

/**
 * Performs the main encoding or decoding operation
 *
 * @param mode Processing mode ("enc" or "dec")
 * @param words Word list for encoding
 * @param in Input stream
 * @param out Output stream
 * @return 0 on success, non-zero on error
 */
static int perform_encoding_decoding(const string_view mode,
                                    vector<small_string>& words,
                                    istream& in, ostream& out)
{
   if (mode == "enc")
   {
      cerr << "encoding the file..." << endl;
      encode(words, in, out);
   }
   else
   {
      unordered_map<small_string, uint16_t> words_rev;
      reverse_words(words, words_rev);

      cerr << "decoding the file..." << endl;
      decode(words_rev, in, out);
   }

   return 0;
}

static int process(int argc, char *argv[])
{
   string_view mode;
   string_view input_file;
   string_view output_file;
   ifstream file_in;
   ofstream file_out;
   istream *in;
   ostream *out;

   // Parse and validate arguments
   if (!parse_arguments(argc, argv, mode, input_file, output_file))
   {
      return 1; // Argument error
   }

   if (mode == "key")
   {
      return handle_key_mode(argc, argv);
   }

   // Set up I/O streams
   if (!setup_io_streams(input_file, output_file, file_in, file_out, in, out))
   {
      return 3; // I/O setup error
   }

   vector<small_string> words = setup_word_list();
   load_static_key();

   return perform_encoding_decoding(mode, words, *in, *out);
}

int (*run)(int argc, char *argv[]) = process;
