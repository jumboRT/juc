#ifndef CONVERTER_HH
#define CONVERTER_HH

#include <Magick++.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <boost/container_hash/hash.hpp>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

const static std::string SEPARATOR = " ";
const static std::string TEX_DIRECTIVE = "tex";
const static std::string MAT_BEGIN_DIRECTIVE = "mat_beg";
const static std::string MAT_PREFIX = "mat_";
const static std::string MAT_INDENT = "    ";
const static std::string MAT_DIFFUSE_DIRECTIVE = "diffuse";
const static std::string MAT_FILTER = "filter";
const static std::string MAT_END_DIRECTIVE = "mat_end";
const static std::string TEX_PREFIX = "tex_";
const static std::string TEX_EXT = ".bmp";
const static std::string FACE_DIRECTIVE = "f";
const static std::string VTN_DIRECTIVE = "x";
const static std::string VT_DIRECTIVE = "w";
const static std::string VN_DIRECTIVE = "y";
const static std::string V_DIRECTIVE = "v";

namespace math {
template <typename T, std::size_t N> using vector = std::array<T, N>;

template <typename T, std::size_t N>
std::size_t hash_value(const vector<T, N> &vector) {
        return boost::hash<std::array<T, N> >(vector);
}
}

struct vertex {
        math::vector<float, 3> point;
        std::optional<math::vector<float, 2> > uv;
        std::optional<math::vector<float, 3> > normal;

        vertex() = default;
        vertex(math::vector<float, 3> point);
        vertex(math::vector<float, 3> point, math::vector<float, 2> uv);
        vertex(math::vector<float, 3> point, math::vector<float, 2> uv,
               math::vector<float, 3> normal);
        vertex(math::vector<float, 3> point,
               std::optional<math::vector<float, 2> > uv,
               std::optional<math::vector<float, 3> > normal);
        vertex(const vertex &other) = default;
        vertex(vertex &&other) noexcept;
        ~vertex() = default;

        vertex &operator=(const vertex &other);
        vertex &operator=(vertex &&other) noexcept;
        bool operator==(const vertex &other) const;

        void swap(vertex &other) noexcept;
};

template <> struct std::hash<vertex> {
        std::size_t operator()(const vertex &vert) const noexcept {
                std::size_t seed = 0;
                boost::hash_combine(seed, vert.point);
                boost::hash_combine(seed, vert.uv);
                boost::hash_combine(seed, vert.normal);
                return seed;
        }
};

class texture_converter {
        Magick::Image _image;

      public:
        const std::filesystem::path from_path, to_path;

        texture_converter() = delete;
        texture_converter(const std::filesystem::path &from,
                          const std::filesystem::path &to);
        texture_converter(const std::string &from, const std::string &to);
        ~texture_converter();

        void convert();
};

class converter {
        std::string _file;
        std::ostream &_out;
        Assimp::Importer _importer;
        const aiScene *const _scene;
        std::unordered_map<std::string, std::string> _textures;
        std::unordered_map<vertex, std::size_t> _vertices;

      public:
        const std::string scene_name;

        converter() = delete;
        converter(const std::string &file, std::ostream &out);

        void convert();

        inline const std::string &get_file() const { return _file; }

      private:
        void write_global_textures();
        void write_materials();
        void write_meshes();

        void write_material(const aiMaterial *material);
        void write_material_diffuse(const aiMaterial *material);
        void write_diffuse_directive(aiColor3D diffuse_color);
        void write_diffuse_directive(aiColor3D diffuse_color,
                                     const std::string &tex_path);
        void write_mesh(const aiMesh *mesh);
        void write_vertex(const vertex &vert);
        void write_face(const aiFace &face);
        void convert_texture(const aiTexture *texture);
        void convert_raw_texture(const aiTexture *texture);
        void convert_compressed_texture(const std::string &path);
        void convert_compressed_texture(const aiTexture *texture);
        static std::string texture_name(const std::string &path);
};

std::ostream &operator<<(std::ostream &stream, const aiColor3D &color);

template <typename T, std::size_t N>
std::ostream &operator<<(std::ostream &stream,
                         const math::vector<T, N> &vector) {
        std::string separator;
        for (auto &&x : vector) {
                stream << separator << x;
                separator = ",";
        }
        return stream;
}
#endif
