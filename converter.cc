#include "converter.hh"
#include <algorithm>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <span>
#include <stdexcept>

vertex::vertex(math::vector<float, 3> point) : point(point) {}

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

void texture_converter::convert() { _image.write(to_path.string()); }

converter::converter(const std::string &file, std::ostream &out, const std::string &name) 
    : _file(file), _out(out), _importer(),
      _scene(_importer.ReadFile(_file.c_str(), aiProcess_Triangulate)),
      scene_name(name) { }

void converter::convert() {
        write_global_textures();
        write_materials();
        write_meshes();
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

void converter::write_meshes() {
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
                            && !_textures.contains(path.C_Str()))
                                convert_compressed_texture(path.C_Str());
                        ++idx;
                }
        }
        _out << MAT_BEGIN_DIRECTIVE << SEPARATOR << MAT_PREFIX
             << material->GetName().C_Str() << std::endl;
        write_material_diffuse(material);
        _out << MAT_END_DIRECTIVE << std::endl;
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
        _out << MAT_INDENT << MAT_DIFFUSE_DIRECTIVE << SEPARATOR << 1
             << SEPARATOR << diffuse_color << std::endl;
}

void converter::write_diffuse_directive(aiColor3D diffuse_color,
                                        const std::string &tex_path) {
        if (tex_path.empty()) {
                write_diffuse_directive(diffuse_color);
        } else {
                _out << MAT_INDENT << MAT_DIFFUSE_DIRECTIVE << SEPARATOR << 1
                     << SEPARATOR << MAT_FILTER << SEPARATOR << TEX_PREFIX
                     << _textures[tex_path] << SEPARATOR << diffuse_color
                     << std::endl;
        }
}

void converter::write_mesh(const aiMesh *mesh) {
        const std::span vertices(mesh->mVertices, mesh->mNumVertices);
        const std::span normals(mesh->mNormals, mesh->mNormals == nullptr
                                                    ? 0
                                                    : mesh->mNumVertices);
        const std::span uvs(
            mesh->mTextureCoords[0],
            mesh->mTextureCoords[0] == nullptr ? 0 : mesh->mNumVertices);

        for (std::size_t idx = 0; idx < vertices.size(); ++idx) {
                vertex vert;
                vert.point
                    = { vertices[idx].x, vertices[idx].y, vertices[idx].z };
                if (uvs.size() > 0) {
                        vert.uv = { uvs[idx].x, uvs[idx].y };
                }
                if (normals.size() > 0) {
                        vert.normal = { normals[idx].x, normals[idx].y,
                                        normals[idx].z };
                }
                if (_vertices.contains(vert)) {
                        continue;
                }
                _vertices[vert] = _vertices.size();
                write_vertex(vert);
        }
        const std::span faces(mesh->mFaces, mesh->mNumFaces);
        // TODO use for_each_n
        for (const aiFace &face : faces) {
                write_face(face);
        }
}

void converter::write_vertex(const vertex &vertex) {
        if (vertex.uv.has_value() && vertex.normal.has_value()) {
                _out << VTN_DIRECTIVE << SEPARATOR << vertex.point << SEPARATOR
                     << vertex.uv.value() << SEPARATOR << vertex.normal.value()
                     << std::endl;
        } else if (vertex.uv.has_value()) {
                _out << VT_DIRECTIVE << SEPARATOR << vertex.point << SEPARATOR
                     << vertex.uv.value() << std::endl;
        } else if (vertex.normal.has_value()) {
                _out << VN_DIRECTIVE << SEPARATOR << vertex.point << SEPARATOR
                     << vertex.normal.value() << std::endl;
        } else {
                _out << V_DIRECTIVE << SEPARATOR << vertex.point << std::endl;
        }
}

void converter::write_face(const aiFace &face) {
        _out << FACE_DIRECTIVE << SEPARATOR << face.mIndices[0] << SEPARATOR
             << face.mIndices[1] << SEPARATOR << face.mIndices[2] << std::endl;
}

void converter::convert_raw_texture(const aiTexture *texture) {
        (void)texture;
        throw std::runtime_error("conversion of embedded textures is "
                                 "currently not implemented");
}

std::string converter::texture_name(const std::string &path) {
        return std::filesystem::path(path).stem().string();
}

std::filesystem::path converter::texture_path(const std::string &name) {
        return std::filesystem::path(scene_name) / (name + TEX_EXT);
}

void converter::convert_compressed_texture(const std::string &tex_path) {
        const std::string name = texture_name(tex_path);
        const std::filesystem::path out_path = texture_path(name);

        _out << TEX_DIRECTIVE << SEPARATOR << TEX_PREFIX << name << SEPARATOR
             << out_path.string() << std::endl;
        texture_converter(tex_path, out_path.string()).convert();
        _textures[tex_path] = name;
}

void converter::convert_compressed_texture(const aiTexture *texture) {
        convert_compressed_texture(texture->mFilename.C_Str());
}

std::ostream &operator<<(std::ostream &stream, const aiColor3D &color) {
        return stream << "(" << color.r << "," << color.g << "," << color.b
                      << ")";
}
