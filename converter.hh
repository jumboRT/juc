#ifndef CONVERTER_HH
#define CONVERTER_HH

#include <string>
#include <iostream>
#include <unordered_map>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <filesystem>
#include <Magick++.h>

const static std::string SEPARATOR = " ";
const static std::string TEX_DIRECTIVE = "tex";
const static std::string TEX_PREFIX = "tex_";
const static std::string TEX_EXT = ".bmp";

class texture_converter {
	Magick::Image _image;
public:
	const std::filesystem::path from_path, to_path;

	texture_converter() = delete;
	texture_converter(const std::filesystem::path &from,
			const std::filesystem::path &to);
	texture_converter(const std::string &from,
			const std::string &to);
	~texture_converter();

	void convert();
};

class converter {
	std::string _file;
	std::ostream &_out;
	Assimp::Importer _importer;
	const aiScene *const _scene;
	std::unordered_map<std::string, std::string> _textures;

public:
	const std::string scene_name;

	converter() = delete;
	converter(const std::string &file, std::ostream &out);

	void convert();

	inline const std::string &get_file() const { return _file; }

private:
	void write_global_textures();

	void convert_texture(const aiTexture *texture);
	void convert_raw_texture(const aiTexture *texture);
	void convert_compressed_texture(const aiTexture *texture);
	static std::string texture_name(const std::string &path);
};

#endif