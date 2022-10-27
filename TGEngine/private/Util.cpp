#include "../public/Util.hpp"
#include <fstream>
#include <algorithm>
#include <string>

namespace tge::util
{

bool exitRequest = false;

std::vector<char>
wholeFile (const fs::path &path)
{
  std::ifstream inputstream (path,
                             std::ios::ate | std::ios::in | std::ios::binary);
  if (!inputstream)
    {
      std::string search = path.generic_string();
      std::transform (search.begin (), search.end (), search.begin (),
                      [] (unsigned char c) {
                        if(c == '\\')
                          return '/';
                        return (char)std::tolower (c); 
                        });
      inputstream = std::ifstream (search);
      if (!inputstream)
        {
#ifdef DEBUG
          printf ("Error couldn't find file: %s!",
                  path.generic_string ().c_str ());
#endif // DEBUG

          throw std::runtime_error (std::string ("File not found: ")
                                    + path.generic_string ());
        }
    }
  const size_t size = (size_t)inputstream.tellg ();
  inputstream.seekg (0, std::ios_base::beg);
  std::vector<char> fileData (size + 1);
  inputstream.read ((char *)fileData.data (), size);
  fileData[size] = 0;
  return fileData;
}

void
requestExit ()
{
  exitRequest = true;
}

} // namespace tge::util