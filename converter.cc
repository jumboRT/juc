#include "converter.hh"
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/texture.h>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>

vertex::vertex(math::vector<float, 3> point) : point(point)  {}

vertex::vertex(math::vector<float, 3> point, math::vector<float, 2> uv)
    : point(point), uv(uv) {}

vertex::vertex(math::vector<float, 3> point, math::vector<float, 2> uv,
               math::vector<float, 3> normal)
    : point(point), uv(uv), normal(normal) {}

vertex::vertex(vertex &&other) noexcept { swap(other); }

vertex &vertex::operator=(const vertex &other) {
        if (this != &other) {
                point = other.point;
                uv = other.uv;
                normal = other.normal;
        }
        return *this;
}

vertex &vertex::operator=(vertex &&other) noexcept {
        swap(other);
        return *this;
}

bool vertex::operator==(const vertex &other) const {
        if (this == &other)
                return true;
        return point == other.point && uv == other.uv
               && normal == other.normal;
};

void vertex::swap(vertex &other) noexcept {
        point.swap(other.point);
        std::swap(uv, other.uv);
        std::swap(normal, other.normal);
}

texture_converter::texture_converter(const std::filesystem::path &from,
                                     const std::filesystem::path &to)
    : _image(), from_path(from), to_path(to) {
        if (!std::filesystem::exists(from))
                throw std::runtime_error(from.string() + ": does not exist");
        if (std::filesystem::status(from).type()
            == std::filesystem::file_type::directory)
                throw std::runtime_error(from.string() + ": is a directory");
        _image.read(from_path.string());

        std::filesystem::path tmp(to);
        std::filesystem::create_directories(tmp.remove_filename());
}

texture_converter::texture_converter(const std::string &from,
                                     const std::string &to)
    : texture_converter(std::filesystem::path(from),
                        std::filesystem::path(to)) {}

texture_converter::~texture_converter() {}

void texture_converter::convert() {
        _image.depth(32);
        _image.colorSpace(Magick::sRGBColorspace);
        _image.alpha(true);
        _image.write(to_path.string());
}

converter::converter(const std::string &file, std::ostream &out,
                     const std::string &name, bool gen_smooth_norm)
    : _file(file), _out(out), _importer(), smooth(gen_smooth_norm),
      _scene(_importer.ReadFile(
          _file.c_str(),
          aiProcess_Triangulate | (aiProcess_GenSmoothNormals * smooth)
              | aiProcess_FlipWindingOrder | aiProcess_JoinIdenticalVertices
              | aiProcess_PreTransformVertices)),
      _pool(12), scene_name(name) {
        if (_scene == nullptr)
                throw std::runtime_error("could not load file");
        _out << std::setiosflags(std::ios_base::fixed);
}

converter::~converter() {
        for (auto &x : _streams) {
                delete x;
        }
}

void converter::convert() {
        write_header();
        write_cameras();
        write_lights();
        write_global_textures();
        write_materials();
        write_node(_scene->mRootNode);
        _pool.join();
        for (auto &x : _streams) {
                for (auto &stream : *x) {
                        _out << stream.view();
                }
        }
}

void converter::write_header() {
        namespace clock = std::chrono;

        const auto now = clock::system_clock::now();
        const std::time_t time = clock::system_clock::to_time_t(now);
        _out << COMMENT_DIRECTIVE << SEPARATOR << "generated by juc on "
             << std::put_time(std::localtime(&time), "%F %T.") << "\n";
}

void converter::write_cameras() {
        bool first = true;
        if (_scene->mNumCameras == 0)
                _out << DEFAULT_CAMERA << "\n";
        std::for_each_n(_scene->mCameras, _scene->mNumCameras,
                        [this, &first](const aiCamera *camera) {
                                if (!first)
                                        _out << COMMENT_DIRECTIVE << SEPARATOR;
                                write_camera(camera);
                                first = false;
                        });
}

void converter::write_lights() {
        // write_all(_scene->mLights, _scene->mNumLights,
        // &converter::write_light);
        std::for_each_n(_scene->mLights, _scene->mNumLights,
                        [this](const aiLight *light) { write_light(light); });
}

void converter::write_camera(const aiCamera *camera) {
        _out << CAMERA_DIRECTIVE << SEPARATOR << camera->mPosition << SEPARATOR
             << camera->mLookAt << SEPARATOR
             << (camera->mHorizontalFOV * (180 / std::acos(0.0))) << "\n";
}

void converter::write_light(const aiLight *light) {
        static bool warned = false;

        if (!warned
            && (light->mType == aiLightSource_DIRECTIONAL
                || light->mType == aiLightSource_POINT
                || light->mType == aiLightSource_SPOT
                || light->mType == aiLightSource_AREA)) {
                std::cerr << "warning: scene contains light a "
                             "directional/point/spot/area or "
                          << "other type of light that has no volume. jumboRT "
                             "currently has no "
                          << "support for lights without a surface area. juc "
                             "will try it's best to "
                          << "convert these lights to jumboRT variants"
                          << "\n";
                warned = true;
        }
        if (light->mType == aiLightSource_AMBIENT) {
                write_light_ambient(light);
        } else if (light->mType == aiLightSource_POINT) {
                write_light_point(light);
        }
}

void converter::write_light_ambient(const aiLight *light) {
        static bool first = true;

        if (!first) {
                _out << COMMENT;
        }
        _out << AMBIENT_LIGHT_DIRECTIVE << SEPARATOR
             << AMBIENT_LIGHT_DEFAULT_BRIGHTNESS << SEPARATOR
             << light->mColorDiffuse << "\n";
}

void converter::write_light_point(const aiLight *light) {
        _out << POINT_LIGHT_DIRECTIVE << SEPARATOR
             << (light->mSize.x * light->mSize.y) << SEPARATOR
             << light->mColorDiffuse << "\n";
}

void converter::write_node(const aiNode *node) {
        const std::span indices(node->mMeshes, node->mNumMeshes);
        /*
        std::for_each_n(node->mMeshes, node->mNumMeshes,
                        [this](unsigned int idx) {
                                write_mesh(_out, _scene->mMeshes[idx]);
                        });
        */
        std::vector<std::ostringstream>* vec =
                new std::vector<std::ostringstream>(node->mNumMeshes);

        std::size_t stream_idx = 0;
        for (std::size_t mesh_idx : indices) {
                const std::size_t vertices_count = _vertices_count;
                boost::asio::post(_pool, [this, vec, stream_idx, mesh_idx,
                                          vertices_count]() {
                        write_mesh(vec->at(stream_idx), _materials,
                                   vertices_count, _scene->mMeshes[mesh_idx]);
                });
                /*
                write_mesh(vec->at(stream_idx), _materials, _vertices_count,
                           _scene->mMeshes[mesh_idx]);
                */
                stream_idx += 1;
                _vertices_count += _scene->mMeshes[mesh_idx]->mNumVertices;
        }
        _streams.push_back(vec);
        
        std::for_each_n(node->mChildren, node->mNumChildren,
                        [this](const aiNode *child) { write_node(child); });
}

void converter::write_global_textures() {
        std::for_each_n(
            _scene->mTextures, _scene->mNumTextures,
            [this](const aiTexture *texture) { convert_texture(texture); });
}

void converter::write_materials() {
        std::for_each_n(
            _scene->mMaterials, _scene->mNumMaterials,
            [this](const aiMaterial *material) { write_material(material); });
}

void converter::convert_texture(const aiTexture *texture) {
        if (texture->mHeight == 0) {
                convert_raw_texture(texture);
        } else {
                convert_compressed_texture(texture);
        }
}

void converter::write_texture(const std::string &scene_name,
                              const std::string &file,
                              const std::string &tex_path) {
        const std::string name = converter::texture_name(tex_path);
        const std::filesystem::path out_path
            = converter::texture_path(scene_name, name);
        std::filesystem::path rel_path
            = std::filesystem::path(file).remove_filename()
              / std::filesystem::path(tex_path);

        texture_converter(rel_path.string(), out_path.string()).convert();
}

void converter::write_material(const aiMaterial *material) {
        for (std::size_t type = 0; type <= AI_TEXTURE_TYPE_MAX; ++type) {
                std::size_t idx = 0;
                while (true) {
                        aiString path;
                        unsigned int uv_idx;
                        if (material->GetTexture(
                                static_cast<aiTextureType>(type), idx, &path,
                                nullptr, &uv_idx, nullptr, nullptr)
                            != AI_SUCCESS) {
                                break;
                        }
                        if (std::string(path.C_Str()).empty() == false
                            && !_textures.contains(path.C_Str())) {
                                boost::asio::post(_pool, [this, path]() {
                                        try {
                                                converter::write_texture(
                                                    scene_name, _file,
                                                    std::string(path.C_Str()));
                                        } catch (const std::exception &ex) {
                                                std::cerr
                                                    << "error: " << ex.what()
                                                    << std::endl;
                                        }
                                });
                                convert_compressed_texture(path.C_Str());
                        }
                        ++idx;
                }
        }
        const std::string name = material->GetName().C_Str();
        _out << MAT_BEGIN_DIRECTIVE << SEPARATOR << MAT_PREFIX << name << "\n";
        write_material_diffuse(material);
        write_material_emissive(material);
        write_material_opacity(material);
        write_material_specular(material);
        if (smooth) {
                _out << MAT_INDENT << MAT_SMOOTH_DIRECTIVE << "\n";
        }
        _out << MAT_END_DIRECTIVE << "\n";
        _materials.push_back(name);
}

void converter::write_material_emissive(const aiMaterial *material) {
        aiColor3D emissive_color;

        material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_color);
        const std::size_t count
            = material->GetTextureCount(aiTextureType_EMISSIVE);
        if (count == 0) {
                write_emissive_directive(emissive_color);
        } else {
                for (std::size_t idx = 0; idx < count; ++idx) {
                        aiString path;
                        material->GetTexture(aiTextureType_EMISSIVE, idx,
                                             &path, nullptr, nullptr, nullptr,
                                             nullptr);
                        write_emissive_directive(emissive_color, path.C_Str());
                }
        }
}

void converter::write_material_opacity(const aiMaterial *material) {
        aiColor4D opacity_color;

        material->Get(AI_MATKEY_OPACITY, opacity_color);
        opacity_color.a = 1.0 - opacity_color.a;
        const std::size_t count
            = material->GetTextureCount(aiTextureType_OPACITY);
        if (count == 0) {
                write_opacity_directive(opacity_color);
        } else {
                for (std::size_t idx = 0; idx < count; ++idx) {
                        aiString path;
                        material->GetTexture(aiTextureType_OPACITY, idx, &path,
                                             nullptr, nullptr, nullptr,
                                             nullptr);
                        write_opacity_directive(opacity_color, path.C_Str());
                }
        }
}

void converter::write_material_specular(const aiMaterial *material) {
        aiColor3D specular_color;

        material->Get(AI_MATKEY_COLOR_SPECULAR, specular_color);
        const std::size_t count
            = material->GetTextureCount(aiTextureType_SPECULAR);
        if (count == 0) {
                write_specular_directive(specular_color);
        } else {
                for (std::size_t idx = 0; idx < count; ++idx) {
                        aiString path;
                        material->GetTexture(aiTextureType_SPECULAR, idx,
                                             &path, nullptr, nullptr, nullptr,
                                             nullptr);
                        write_specular_directive(specular_color, path.C_Str());
                }
        }
}

void converter::write_material_diffuse(const aiMaterial *material) {
        aiColor3D diffuse_color;

        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
        const std::size_t count
            = material->GetTextureCount(aiTextureType_DIFFUSE);
        if (count == 0) {
                write_diffuse_directive(diffuse_color);
                return;
        }
        for (std::size_t idx = 0; idx < count; ++idx) {
                aiString path;
                material->GetTexture(aiTextureType_DIFFUSE, idx, &path,
                                     nullptr, nullptr, nullptr, nullptr);
                write_diffuse_directive(diffuse_color, path.C_Str());
        }
}

void converter::write_diffuse_directive(aiColor3D diffuse_color) {
        _out << MAT_INDENT << MAT_DIFFUSE_DIRECTIVE << SEPARATOR
             << BXDF_DEFAULT_WEIGHT << SEPARATOR << diffuse_color << "\n";
}

void converter::write_emissive_directive(aiColor3D emissive_color) {
	if (emissive_color.r == 0.0f && emissive_color.g == 0.0f
			&& emissive_color.b == 0.0f)
		return;
        _out << MAT_INDENT << MAT_EMISSIVE_DIRECTIVE << SEPARATOR
             << MAT_DEFAULT_BRIGHTNESS << SEPARATOR << emissive_color << "\n";
}

void converter::write_opacity_directive(aiColor4D opacity_color) {
	if (opacity_color.a == 1.0f)
		return;
        _out << MAT_INDENT << MAT_OPACITY_DIRECTIVE << SEPARATOR
             << opacity_color << "\n";
}

void converter::write_specular_directive(aiColor3D specular_color) {
	if (specular_color.r == 0.0f && specular_color.g == 0.0f
			&& specular_color.b == 0.0f)
		return;
        _out << MAT_INDENT << MAT_SPECULAR_DIRECTIVE << SEPARATOR
             << BXDF_DEFAULT_WEIGHT << SEPARATOR << specular_color
             << "\n";
}

void converter::write_diffuse_directive(aiColor3D diffuse_color,
                                        const std::string &tex_path) {
        if (tex_path.empty()) {
                write_diffuse_directive(diffuse_color);
        } else {
                _out << MAT_INDENT << MAT_DIFFUSE_DIRECTIVE << SEPARATOR
                     << BXDF_DEFAULT_WEIGHT << SEPARATOR << MAT_FILTER
                     << SEPARATOR << TEX_PREFIX << _textures[tex_path]
                     << SEPARATOR << diffuse_color << "\n";
        }
}

void converter::write_emissive_directive(aiColor3D emissive_color,
                                         const std::string &tex_path) {
	if (emissive_color.r == 0.0f && emissive_color.g == 0.0f
			&& emissive_color.b == 0.0f)
		return;
        if (tex_path.empty()) {
                write_emissive_directive(emissive_color);
        } else {
                _out << MAT_INDENT << MAT_EMISSIVE_DIRECTIVE << SEPARATOR
                     << MAT_DEFAULT_BRIGHTNESS << SEPARATOR << MAT_FILTER
                     << SEPARATOR << TEX_PREFIX << _textures[tex_path]
                     << SEPARATOR << emissive_color << "\n";
        }
}

void converter::write_opacity_directive(aiColor4D opacity_color,
                                        const std::string &tex_path) {
	if (opacity_color.a == 1.0f)
		return;
        if (tex_path.empty()) {
                write_opacity_directive(opacity_color);
        } else {
                _out << MAT_INDENT << MAT_OPACITY_DIRECTIVE << SEPARATOR
                     << MAT_FILTER << SEPARATOR << TEX_PREFIX
                     << _textures[tex_path] << SEPARATOR << opacity_color
                     << "\n";
        }
}

void converter::write_specular_directive(aiColor3D specular_color,
                                         const std::string &tex_path) {
	if (specular_color.r == 0.0f && specular_color.g == 0.0f
			&& specular_color.b == 0.0f)
		return;
        if (tex_path.empty()) {
                write_specular_directive(specular_color);
        } else {
                _out << MAT_INDENT << MAT_EMISSIVE_DIRECTIVE << SEPARATOR
                     << BXDF_DEFAULT_WEIGHT << SEPARATOR << MAT_FILTER
                     << SEPARATOR << TEX_PREFIX << _textures[tex_path]
                     << SEPARATOR << specular_color << "\n";
        }
}

void converter::write_mesh(std::ostream &stream,
                           const std::vector<std::string> &materials,
                           std::size_t face_offset, const aiMesh *mesh) {
        const std::span vertices(mesh->mVertices, mesh->mNumVertices);
        const std::span normals(mesh->mNormals, mesh->mNormals == nullptr
                                                    ? 0
                                                    : mesh->mNumVertices);
        const std::span uvs(
            mesh->mTextureCoords[0],
            mesh->mTextureCoords[0] == nullptr ? 0 : mesh->mNumVertices);
        stream << MAT_USE_DIRECTIVE << SEPARATOR << MAT_PREFIX
               << materials[mesh->mMaterialIndex] << "\n";
        for (std::size_t idx = 0; idx < vertices.size(); ++idx) {
                const aiVector3D point = vertices[idx];
                vertex vert;
                vert.point = { point.x, point.z, point.y };
                if (uvs.size() > 0) {
                        vert.uv = { uvs[idx].x, uvs[idx].y };
                }
                if (normals.size() > 0) {
                        const aiVector3D normal = normals[idx];
                        vert.normal = { normal.x, normal.z, normal.y };
                }
                write_vertex(stream, vert);
        }
        std::for_each_n(mesh->mFaces, mesh->mNumFaces,
                        [&stream, face_offset](const aiFace &face) {
                                write_face(stream, face_offset, face);
                        });
}

void converter::write_vertex(std::ostream &stream, const vertex &vertex) {
        stream << VTN_DIRECTIVE << SEPARATOR << vertex.point << SEPARATOR
               << vertex.uv << SEPARATOR << vertex.normal << "\n";
}

void converter::convert_raw_texture(const aiTexture *texture) {
        (void)texture;
        throw std::runtime_error("conversion of embedded textures is "
                                 "currently not implemented");
}

std::string converter::texture_name(const std::string &path) {
        return std::filesystem::path(path).stem().string();
}

std::filesystem::path converter::texture_path(const std::string &scene_name,
                                              const std::string &name) {
        return std::filesystem::path(scene_name) / (name + TEX_EXT);
}

std::filesystem::path converter::texture_path(const std::string &name) {
        return texture_path(scene_name, name);
}

void converter::convert_compressed_texture(const std::string &tex_path) {
        const std::string name = texture_name(tex_path);
        const std::filesystem::path out_path = texture_path(name);
        std::filesystem::path rel_path
            = std::filesystem::path(_file).remove_filename()
              / std::filesystem::path(tex_path);

        _out << TEX_DIRECTIVE << SEPARATOR << TEX_PREFIX << name << SEPARATOR
             << out_path.string() << "\n";
        // texture_converter(rel_path.string(), out_path.string()).convert();
        _textures[tex_path] = name;
}

void converter::convert_compressed_texture(const aiTexture *texture) {
        convert_compressed_texture(texture->mFilename.C_Str());
}

std::ostream &operator<<(std::ostream &stream, const better_float &fl) {
        std::ostringstream ss;
        ss << std::fixed << fl.value();
        const std::string str = ss.str();
	std::string_view view = str;
	view = view.substr(0, view.find_last_not_of("0") + 1);
	if (view.ends_with('.'))
		view = view.substr(0, view.length() - 1);
        return stream << view;
}

std::ostream &operator<<(std::ostream &stream, const aiColor3D &color) {
        return stream << "(" << better_float(color.r) << ","
                      << better_float(color.g) << "," << better_float(color.b)
                      << ")";
}

std::ostream &operator<<(std::ostream &stream, const aiColor4D &color) {
        return stream << "(" << better_float(color.r) << ","
                      << better_float(color.g) << "," << better_float(color.b)
                      << "," << better_float(color.a) << ")";
}

std::ostream &operator<<(std::ostream &stream, const aiVector3D &vec) {
        return stream << better_float(vec.x) << "," << better_float(vec.y)
                      << "," << better_float(vec.z);
}

std::ostream &operator<<(std::ostream &stream,
                         const math::vector<float, 2> &vec) {
        return stream << better_float(vec[0]) << ","
                      << better_float(vec[1]);
}

std::ostream &operator<<(std::ostream &stream,
                         const math::vector<float, 3> &vec) {
        return stream << better_float(vec[0]) << ","
                      << better_float(vec[1]) << ","
                      << better_float(vec[2]);
}
