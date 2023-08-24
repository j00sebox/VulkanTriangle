#include "config.hpp"
#include "Utility.hpp"

namespace util
{
    std::vector<u8> read_binary_file(const std::string& file_name)
    {
        // ate: start reading at the end of the file
        // binary: read file as a binary to avoid text transformations
        std::ifstream file(file_name, std::ios::ate | std::ios::binary);

        if(!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        size_t file_size = (size_t)file.tellg();
        std::vector<u8> buffer(file_size);

        // go back to beginning of file and read it
        file.seekg(0);
        file.read(buffer.data(), file_size);

        file.close();

        return buffer;
    }

    void write_binary_file(void* data, size_t size, const char* file_name)
    {
        std::ofstream  file(file_name, std::ios::binary);

        if(!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        file.write((char*)data, size);

        file.close();
    }

    namespace spirv
    {
        // an id in spirv is a label used to refer to an object, type, a function, etc.
        // always consumes one word
        struct Id
        {
            spv::Op op;
            u32 set;
            u32 binding;

            // type of the id
            // this acts like an index into the id array
            u32 type;

            // used for vector and matrix types
            u32 count;

            // type of memory that is holding the data
            // eg. Uniform, Input, Image, etc.
            spv::StorageClass storage_class;
        };

        vk::ShaderStageFlags parse_execution_model(spv::ExecutionModel execution_model)
        {
            switch (execution_model)
            {
                case (spv::ExecutionModelVertex):
                {
                    return vk::ShaderStageFlagBits::eVertex;
                }
                case (spv::ExecutionModelGeometry):
                {
                    return vk::ShaderStageFlagBits::eGeometry;
                }
                case (spv::ExecutionModelFragment):
                {
                    return vk::ShaderStageFlagBits::eFragment;
                }
                case (spv::ExecutionModelKernel):
                {
                    return vk::ShaderStageFlagBits::eCompute;
                }
            }

            return vk::ShaderStageFlagBits::eAll;
        }

        void parse_binary(const u32* spirv_data, u32 size, ParseResult& result)
        {
            // check we are reading valid SPIR-V data
            // first 4 bytes need to match this identifier for it to be a SPIR-V binary
            u32 spirv_identifier = 0x07230203;
            assert(spirv_identifier == spirv_data[0]);

            // calculate number of 32-bit words in the binary
            u32 spv_word_count = size / 4;

            // basically how many entries this binary has
            u32 num_ids = spirv_data[3];

            vk::ShaderStageFlags stage_flag;
            Id ids[num_ids];

            // loop over the rest of the words in the binary
            for(u32 word_index = 5; word_index < spv_word_count;)
            {
                // the op type is stored in the bottom byte
                auto op = (spv::Op)(spirv_data[word_index] & 0xFF);

                // this is the number of words belonging to this operation
                u16 word_count = (u16)(spirv_data[word_index] >> 16);

                switch (op)
                {
                    case (spv::OpEntryPoint):
                    {
                        // can figure out what kind of shader we are dealing with
                        auto model = (spv::ExecutionModel)spirv_data[word_index + 1];
                        stage_flag = parse_execution_model(model);
                        break;
                    }
                    case (spv::OpDecorate):
                    {
                        u32 id_index = spirv_data[word_index + 1];

                        Id& id = ids[id_index];

                        // decorations are extra info added to ids or structs
                        auto decoration = (spv::Decoration)spirv_data[word_index + 2];

                        switch(decoration)
                        {
                            case (spv::DecorationBinding):
                            {
                                id.binding = spirv_data[word_index + 3];
                                break;
                            }
                            case (spv::DecorationDescriptorSet):
                            {
                                id.set = spirv_data[word_index + 3];
                                break;
                            }
                        }

                        break;
                    }
                    case (spv::OpTypeVector):
                    {
                        u32 id_index = spirv_data[word_index + 1];

                        Id& id = ids[id_index];
                        id.op = op;
                        id.type = spirv_data[word_index + 2];
                        id.count = spirv_data[word_index + 3];

                        break;
                    }
                    case (spv::OpTypeSampler):
                    {
                        u32 id_index = spirv_data[word_index + 1];

                        Id& id = ids[id_index];
                        id.op = op;

                        break;
                    }
                    // a variable always refers to a pointer of some kind
                    case (spv::OpVariable):
                    {
                        u32 id_index = spirv_data[word_index + 2];

                        Id& id = ids[id_index];
                        id.op = op;
                        id.type = spirv_data[word_index + 1];
                        id.storage_class = (spv::StorageClass)spirv_data[word_index + 3];

                        break;
                    }
                }

                word_index += word_count;
            }

            // now we loop through all the ids and pick out the relevant ones
            for(u32 id_index = 0; id_index < num_ids; ++id_index)
            {
                Id& id = ids[id_index];

                if(id.op == spv::OpVariable)
                {
                    if(id.storage_class == spv::StorageClassUniform || id.storage_class == spv::StorageClassUniformConstant)
                    {
                        Id& uniform_type = ids[ids[id.type].type];

                        DescriptorSetLayoutCreationInfo& set_layout = result.sets[id.set];

                        set_layout.set_index = id.set;

                        DescriptorSetLayoutCreationInfo::Binding binding{};
                        binding.start = id.binding;
                        binding.count = 1;

                        switch(uniform_type.op)
                        {
                            case (spv::OpTypeStruct):
                            {
                                binding.type = vk::DescriptorType::eUniformBuffer;
                                break;
                            }
                            case (spv::OpTypeSampledImage):
                            {
                                binding.type = vk::DescriptorType::eCombinedImageSampler;
                                break;
                            }
                        }

                        set_layout.bindings[id.binding] = binding;
                    }
                }
            }
        }
    }
}

