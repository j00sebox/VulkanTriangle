#include <string>
#include <vector>
#include <fstream>

namespace Util
{
    // reads all bytes from SPIR-V bin and return byte array
    static std::vector<char> read_file(const std::string& file_name)
    {
        // ate: start reading at the end of the file
        // binary: read file as a binary to avoid text transformations
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);

        if(!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<char> buffer(file_size);

        // go back to beginning of file and read it
        file.seekg(0);
        file.read(buffer.data(), file_size);

        file.close();

        return buffer;
    }
}