#include "converter.hh"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>

texture_converter::texture_converter(const std::filesystem::path &from,
                                     const std::filesystem::path &to)
    : _image(), from_path(from), to_path(to) {
        if (!std::filesystem::exists(from))
                throw std::runtime_error(from.string() + ": does not exist");
        if (std::filesystem::status(from).type()
            == std::filesystem::file_type::directory)
                throw std::runtime_error(from.string() + ": is a directory");
        _image.read(from_path.string());
}

texture_converter::texture_converter(const std::string &from,
                                     const std::string &to)
    : texture_converter(std::filesystem::path(from),
                        std::filesystem::path(to)) {}

texture_converter::~texture_converter() {}

void texture_converter::convert() { _image.write(to_path.string()); }

converter::converter(const std::string &file, std::ostream &out)
    : _file(file), _out(out), _importer(),
      _scene(_importer.ReadFile(_file.c_str(), aiProcess_Triangulate)) {}

void converter::convert() {
        write_global_textures();
        write_materials();
}

void converter::write_global_textures() {
        const std::span textures(_scene->mTextures, _scene->mNumTextures);

        // TODO make texture const
        for (aiTexture *texture : textures) {
                convert_texture(texture);
        }
}

void converter::write_materials() {
        const std::span materials(_scene->mMaterials, _scene->mNumMaterials);

        for (const aiMaterial *material : materials) {
                write_material(material);
        }
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
        _out << MAT_BEGIN_DIRECTIVE << SEPARATOR
             << material->GetName().C_Str() << std::endl;

        _out << MAT_END_DIRECTIVE << std::endl;
}

void converter::convert_raw_texture(const aiTexture *texture) {
        (void)texture;
        throw std::runtime_error(
            "conversion of embedded textures is currently not implemented");
}

std::string converter::texture_name(const std::string &path) {
        return std::filesystem::path(path).stem().string();
}

void converter::convert_compressed_texture(const std::string &tex_path) {
        const std::string name = texture_name(tex_path);

        std::filesystem::path out_path
            = std::filesystem::current_path() / scene_name / (name + TEX_EXT);
        _out << TEX_DIRECTIVE << SEPARATOR << TEX_PREFIX << name << SEPARATOR
             << out_path.string() << std::endl;
        texture_converter(tex_path, out_path.string()).convert();
        _textures[tex_path] = name;
}

void converter::convert_compressed_texture(const aiTexture *texture) {
        convert_compressed_texture(texture->mFilename.C_Str());
}
