#include "GPUResources.hpp"

#include <string>
#include <vector>
#include <spirv.hpp>
#include <vulkan/vulkan.hpp>

namespace util
{
    // reads all bytes from binaries like SPIR-V returns a byte array
    std::vector<u8> read_binary_file(const std::string& file_name);
    void write_binary_file(void* data, size_t size, const char* file_name);

    std::vector<std::string> read_file_to_vector(const std::string& filename);

    // SPIR-V parsing
    namespace spirv
    {
        static const u32 MAX_SET_COUNT = 32;

        struct ParseResult
        {
            u32                         set_count;
            DescriptorSetLayoutCreationInfo sets[MAX_SET_COUNT];
        };

        void parse_binary(const u32* spirv_data, u32 size, ParseResult& result);
        vk::ShaderStageFlags parse_execution_model(spv::ExecutionModel execution_model);
    }
}