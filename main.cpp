#include <exception>
#include <iostream>

extern int (*run)(int argc, char *argv[]);

int main(int argc, char *argv[])
{
   std::ios::sync_with_stdio(false);
   try
   {
      return run(argc, argv);
   }
   catch (const std::exception& exc)
   {
      std::cerr << "error: " << exc.what() << '\n';
      return 0xf;
   }
}
