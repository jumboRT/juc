#ifndef CONVERTER_HH
#define CONVERTER_HH

#include <Magick++.h>
#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <boost/container_hash/hash.hpp>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

const static std::string SEPARATOR = " ";
const static std::string COMMENT_DIRECTIVE = "#";
const static std::string COMMENT = COMMENT_DIRECTIVE + SEPARATOR;
const static std::string CAMERA_DIRECTIVE = "C";
const static std::string AMBIENT_LIGHT_DIRECTIVE = "A";
const static std::string AMBIENT_LIGHT_DEFAULT_BRIGHTNESS = "";
const static std::string POINT_LIGHT_DIRECTIVE = "l";
const static std::string TEX_DIRECTIVE = "tex_def";
const static std::string MAT_USE_DIRECTIVE = "mat_use";
const static std::string MAT_BEGIN_DIRECTIVE = "mat_beg";
const static std::string MAT_PREFIX = "mat_";
const static std::string MAT_INDENT = "    ";
const static std::string MAT_DIFFUSE_DIRECTIVE = "diffuse";
const static std::string MAT_EMISSIVE_DIRECTIVE = "emission";
const static std::string MAT_OPACITY_DIRECTIVE = "alpha";
const static std::string MAT_SPECULAR_DIRECTIVE
    = "reflective"; // TODO make this the proper specular directive
const static std::string MAT_SPECULAR_DEFAULT_FUZZY = "0.5";
const static std::string MAT_DEFAULT_BRIGHTNESS = "1.0";
const static std::string BXDF_DEFAULT_WEIGHT = "1.0";
const static std::string MAT_FILTER = "filter";
const static std::string MAT_SMOOTH_DIRECTIVE = "smooth";
const static std::string MAT_END_DIRECTIVE = "mat_end";
const static std::string TEX_PREFIX = "tex_";
const static std::string TEX_EXT = ".bmp";
const static std::string FACE_DIRECTIVE = "f";
const static std::string VTN_DIRECTIVE = "x";
const static std::string VT_DIRECTIVE = "w";
const static std::string VN_DIRECTIVE = "y";
const static std::string V_DIRECTIVE = "v";

const static std::string DEFAULT_CAMERA
    = CAMERA_DIRECTIVE + SEPARATOR + "0,0,0 1,0,0 90";

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
        bool smooth;
        const aiScene *const _scene;
        std::unordered_map<std::string, std::string> _textures;
        std::unordered_map<vertex, std::size_t> _vertices;
        std::vector<std::string> _materials;
        std::size_t _triangles = 0;

      public:
        const std::string scene_name;

        converter() = delete;
        converter(const std::string &file, std::ostream &out,
                  const std::string &name, bool gen_smooth_norm);

        void convert();

        inline const std::string &get_file() const { return _file; }

      private:
        void write_global_textures();
        void write_cameras();
        void write_lights();
        void write_materials();
        void write_header();

        /*
        template<typename T, typename R>
        void write_all(T *array, std::size_t count, R (converter::* proc)(T)) {
                std::for_each_n(array, count, [this, proc](T t) {
                                (*this.*proc)(t);
                                });
        }
        */

        void write_camera(const aiCamera *camera);
        void write_light(const aiLight *light);
        void write_light_ambient(const aiLight *light);
        void write_light_point(const aiLight *light);

        void write_material(const aiMaterial *material);
        void write_material_diffuse(const aiMaterial *material);
        void write_diffuse_directive(aiColor3D diffuse_color);
        void write_diffuse_directive(aiColor3D diffuse_color,
                                     const std::string &tex_path);
        void write_material_emissive(const aiMaterial *material);
        void write_emissive_directive(aiColor3D emissive_color);
        void write_emissive_directive(aiColor3D emissive_color,
                                      const std::string &tex_path);
        void write_material_opacity(const aiMaterial *material);
        void write_opacity_directive(aiColor3D opacity_color);
        void write_opacity_directive(aiColor3D opacity_color,
                                      const std::string &tex_path);
        void write_material_specular(const aiMaterial *material);
        void write_specular_directive(aiColor3D specular_color);
        void write_specular_directive(aiColor3D specular_color,
                                      const std::string &tex_path);

        void write_node(const aiNode *node);
        void write_mesh(const aiMesh *mesh, const aiMatrix4x4 &transformation);
        void write_vertex(const vertex &vert);
        void write_face(const aiFace &face,
                        const std::vector<vertex> &indices);
        void convert_texture(const aiTexture *texture);
        void convert_raw_texture(const aiTexture *texture);
        void convert_compressed_texture(const std::string &path);
        void convert_compressed_texture(const aiTexture *texture);
        std::filesystem::path texture_path(const std::string &name);
        static std::string texture_name(const std::string &path);
};

std::ostream &operator<<(std::ostream &stream, const aiColor3D &color);
std::ostream &operator<<(std::ostream &stream, const aiVector3D &vec);

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
