#include "config.hpp"
#include "Primitives.hpp"

#include <cstring>

std::string primitive_type_to_str(PrimitiveTypes primitve_type)
{
    switch (primitve_type)
    {
        case PrimitiveTypes::None:
            return "none";
        case PrimitiveTypes::Cube:
            return "cube";
        case PrimitiveTypes::Quad:
            return "quad";
        default:
            return "none";
    }
}

PrimitiveTypes str_to_primitive_type(const char* name)
{
    if (!strcmp(name, "cube"))
    {
        return PrimitiveTypes::Cube;
    }
    else if (!strcmp(name, "quad"))
    {
        return PrimitiveTypes::Quad;
    }

    return PrimitiveTypes::None;
}

const std::vector<Vertex> Cube::vertices = {
        //        7--------6
        //       /|       /|
        //      4--------5 |
        //      | |      | |
        //      | 3------|-2
        //      |/       |/
        //      0--------1

        // position          normal              tex coords

        // front
        { { -1.f, -1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
        { { 1.f, -1.f, 1.f },  { 0.f, 0.f, 1.f }, { 1.f, 0.f } },
        { { 1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
        { { -1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },

        // back
        { { 1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 0.f } },
        { { -1.f, -1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f } },
        { { -1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 1.f, 1.f } },
        { { 1.f, 1.f, -1.f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f } },

        // right
        { { 1.f, -1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f } },
        { { 1.f, -1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f } },
        { { 1.f, 1.f, -1.f }, { 1.f, 0.f, 0.f }, { 1.f, 1.f } },
        { { 1.f, 1.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f } },

        // left
        { { -1.f, -1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 0.f } },
        { { -1.f, -1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 0.f } },
        { { -1.f, 1.f, 1.f }, { -1.f, 0.f, 0.f }, { 1.f, 1.f } },
        { { -1.f, 1.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f } },

        // top
        { { -1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f } },
        { { 1.f, 1.f, 1.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },
        { { 1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 1.f, 1.f } },
        { { -1.f, 1.f, -1.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f } },

        // bottom
        { { 1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 0.f, 0.f } },
        { { -1.f, -1.f, 1.f }, { 0.f, -1.f, 0.f }, { 1.f, 0.f } },
        { { -1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f } },
        { { 1.f, -1.f, -1.f }, { 0.f, -1.f, 0.f }, { 0.f, 1.f } }
};

const std::vector<u32> Cube::indices = {

        // front
        0, 1, 2,
        0, 2, 3,

        // back
        4, 5, 6,
        4, 6, 7,

        // right
        8, 9, 10,
        8, 10, 11,

        // left
        12, 13, 14,
        12, 14, 15,

        // top
        16, 17, 18,
        16, 18, 19,

        // bottom
        20, 21, 22,
        20, 22, 23
};

const std::vector<Vertex> Quad::vertices = {
        { { -0.5f, -0.5f, -0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f } },
        { { -0.5f, 0.5f, -0.5f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f } },
        { { 0.5f, 0.5f, -0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 1.f } },
        { { 0.5f, -0.5f, -0.5f }, { 0.f, 0.f, 1.f }, { 1.f, 0.f } }
};

const std::vector<unsigned int> Quad::indices = {
        0, 2, 1,
        0, 3, 2
};
