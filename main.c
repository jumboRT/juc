#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <MagickWand/MagickWand.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MAX_ERROR_LENGTH 1024
#define TEX_EXTENSION ".bmp"

#define TEX_DIRECTIVE "tex"
#define MAT_BEGIN_DIRECTIVE "mat_beg"
#define MAT_END_DIRECTIVE "mat_end"

#define TEX_PREFIX "tex_"
#define MAT_PREFIX "mat_"

static char error[MAX_ERROR_LENGTH];

void write_error(char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	vsnprintf(error, MAX_ERROR_LENGTH, fmt, args);
	va_end(args);
}

int jc_asprintf(char **dst, char *fmt, ...)
{
	va_list args;
	int count;

	va_start(args, fmt);
	count = vasprintf(dst, fmt, args);
	va_end(args);
	if (count < 0)
		write_error("%s", "could not vasprintf");
	return count;
}

int jc_fprintf(FILE *file, char *fmt, ...)
{
	va_list args;
	int count;

	va_start(args, fmt);
	count = vfprintf(file, fmt, args);
	va_end(args);
	if (count < 0)
		write_error("%s", "could not vfprintf");
	return count;
}

char* jc_strdup(const char *str)
{
	char *result = strdup(str);
	if (result == NULL)
		write_error("%s", "failed to allocate memory");
	return (result);
}

char* jc_strndup(const char *str, size_t len)
{
	char *result = strndup(str, len);
	if (result == NULL)
		write_error("%s", "failed to allocate memory");
	return (result);
}

char* jc_basename(const char *basename)
{
	const char *start = strrchr(basename, '\\');
	const size_t len = strlen(basename);
	return jc_strndup(start, len - (start - basename));
}

FILE* jc_fopen(const char *restrict path, const char *restrict mode)
{
	FILE *result = fopen(path, mode);
	if (result == NULL)
		write_error("%s", strerror(errno));
	return (result);
}

int write_compressed_texture(FILE *out, const C_STRUCT aiTexture *tex)
{
	MagickWand *mw = NewMagickWand();
	int rc = -1;

	if (mw == NULL) {
		write_error("%s", "could not create magick wand");
		goto cleanup;
	}

	if (MagickReadImage(mw, tex->mFilename.data) == MagickFalse
	    || MagickSetImageFormat(mw, "bmp") == MagickFalse
	    || MagickWriteImageFile(mw, out) == MagickFalse)
	{
		ExceptionType severity;
		write_error("%s", MagickGetException(mw, &severity));
		goto cleanup;
	}
	rc = 0;
cleanup:
	if (mw != NULL)
		ClearMagickWand(mw);
	return (rc);
}

int write_raw_texture(FILE *out, const C_STRUCT aiTexture *tex)
{
	(void) out;
	(void) tex;
	write_error("%s", "embedded textures are not implemented yet");
	return (-1);
}

/* basename without extension of the file_name */
char *file_stem(const char *file_name)
{
	const char *base_name = jc_basename(file_name);
	if (base_name == NULL)
		return (NULL);
	const char *dot = strrchr(base_name, '.');
	if (dot == NULL)
		return jc_strdup(base_name);
	return jc_strndup(base_name, dot - base_name);
}

char *trim_ws(char *str)
{
	char *const result = str;
	while (*str != '\0')
	{
		if (isspace(*str))
		{
			*str = '_';
		}
		str += 1;
	}
	return (result);
}

int write_texture(FILE *out, const char *dir, const C_STRUCT aiTexture *tex) {
	char *stem = NULL;
	char *new_file = NULL;
	FILE *tex_file = NULL;
	int rc = -1;

	if ((stem = file_stem(tex->mFilename.data)) == NULL
	    || jc_asprintf(&new_file, "%s/%s.%s", dir, stem, TEX_EXTENSION) < 0
	    || (tex_file = jc_fopen(new_file, "w+")) == NULL)
		goto cleanup;

	trim_ws(stem);
	if (jc_fprintf(out, "%s %s%s %s", TEX_DIRECTIVE, TEX_PREFIX, stem, tex_file) < 0)
		goto cleanup;
	if (tex->mHeight == 0) {
		rc = write_compressed_texture(tex_file, tex);
	} else {
		rc = write_raw_texture(tex_file, tex);
	}
cleanup:
	free(stem);
	free(new_file);
	if (tex_file != NULL)
		fclose(tex_file);
	return (rc);
}

int write_textures(FILE *out, const C_STRUCT aiScene *scene)
{
	unsigned int idx = 0;

	while (idx < scene->mNumTextures)
	{
		if (write_texture(out, "textures", scene->mTextures[idx]))
			return -1;
		++idx;
	}
	return 0;
}

int write_material_textures(FILE *out, const char *dir, const C_STRUCT aiMaterial *mat)
{
	C_STRUCT aiString path;
	for (unsigned int type = 0; type <= AI_TEXTURE_TYPE_MAX; ++type) {
		unsigned int idx = 0;
			if (aiGetMaterialTexture(mat, (C_ENUM aiTextureType) type, idx, &
}

int write_material(FILE *out, const char *dir, const C_STRUCT aiMaterial *mat)
{
	C_STRUCT aiString name;

	if (aiGetMaterialString(mat, AI_MATKEY_NAME, &name) == aiReturn_FAILURE) {
		write_error("%s", "empty material names are not allowed");
		return -1;
	}
	printf("%s %s%.*s\n", MAT_BEGIN_DIRECTIVE, MAT_PREFIX, name.length, name.data);
	printf("%s\n", MAT_END_DIRECTIVE);
	return 0;
}

int write_materials(FILE *out, const C_STRUCT aiScene *scene)
{
	for (size_t idx = 0; idx < scene->mNumMaterials; ++idx) {
		if (write_material(out, "textures", scene->mMaterials[idx]))
			return -1;
	}
	return 0;
}

int
	convert_scene(const C_STRUCT aiScene *scene)
{
	if (write_textures(stdout, scene))
		return -1;
	if (write_materials(stdout, scene))
		return -1;
	/*
	if (write_meshes(stdout, scene, error))
		return (-1);
	*/
	return (0);
}

int convert(const char *file)
{
	const C_STRUCT aiScene *scene;
	int rc;

	scene = aiImportFile(file, aiProcess_Triangulate);
	if (scene == NULL) {
		write_error("%s", aiGetErrorString());
		return (-1);
	}
	rc = convert_scene(scene);
	aiReleaseImport(scene);
	return (rc);
}

void print_usage(const char *name)
{
	fprintf(stdout, "%s <scene_file>\n", name);
}

int
	main(int argc, char *argv[])
{
	const char *ext;
	
	if (argc != 2)
	{
		print_usage(argv[0]);
		return (EXIT_SUCCESS);
	}
	ext = strrchr(argv[1], '.');
	if (ext == NULL)
	{
		fprintf(stderr,
			"%s: %s: could not deduce file type\n", argv[0], argv[1]);
		return (EXIT_FAILURE);
	}	
	if (aiIsExtensionSupported(ext) == AI_FALSE)
	{
		fprintf(stderr,
			"%s: %s: file type is not supported\n", argv[0], argv[1]);
		return (EXIT_FAILURE);
	}
	if (convert(argv[1]) == 0)
		return (EXIT_SUCCESS);
	fprintf(stderr, "%s: %.*s\n", argv[0], MAX_ERROR_LENGTH, error);
	return (EXIT_FAILURE);
}
