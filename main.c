#include <assimp/cimport.h>
#include <assimp/scene.h>

#include <stdlib.h>
#include <stdio.h>

int write_compressed_texture(FILE *out, const aiTexture *tex, char **error)
{
	MagickWand *mw = NewMagickWand();
	int rc = -1;

	if (mw == NULL) {
		jc_snprintf(error, "%s", "could not create magick wand");
		goto cleanup;
	}

	if (MagickReadImage(mw, tex->mFileName) == MagickFalse
	    || MagickSetImageFormat(mw, "bmp") == MagickFalse
	    || MagickWriteImageFile(mw, out) == MagickFalse)
	{
		ExceptionType severity;
		jc_snprintf(error, "%s", MagickGetException(mw, &severity));
		goto cleanup;
	}
	rc = 0;
cleanup:
	if (mw != NULL)
		ClearMagickWand(wand);
	return (rc);
}

/* basename without extension of the file_name */
char *file_stem(const char *file_name)
{
	const char *base_name = basename(file_name);
	if (base_name == NULL) {
		write_error(error, "%s", strerror(errno));
		return (-1);
	}
	const char *dot = strrchr(base_name, '.');
	if (dot == NULL)
		return jc_strdup(base_name);
	return jc_strndup(base_name, dot - base_name);
}

int write_texture(FILE *out, const char *dir, const aiTexture *tex) {
	char *stem = NULL;
	char *new_file = NULL;
	FILE *tex_file = NULL;
	int rc = -1;

	if ((stem = file_stem(tex->mFileName)) == NULL
	    || jc_asprintf(&new_file, "%s/%s.%s", dir, stem, TEX_EXTENSION) < 0
	    || (tex_file = jc_fopen(new_file)) == NULL)
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

int write_textures(FILE *out, const C_STRUCT aiScene *scene) {
	unsigned int idx = 0;

	while (idx < scene->mNumTextures)
	{
		if (write_texture(out, scene->mTextures[idx]))
			return (-1);
		++idx;
	}
	return (0);
}

int
	convert_scene(const C_STRUCT aiScene *scene)
{
	if (write_textures(stdout, scene))
		return (-1);
	/*
	if (write_materials(stdout, scene, error))
		return (-1);
	if (write_meshes(stdout, scene, error))
		return (-1);
	*/
	return (0);
}

int convert(const char *file)
{
	const C_STRUCT aiScene *scene;
	int rc;

	scene = aiImportFile(path, aiProcess_Triangulate);
	if (scene == NULL) {
		if (error != NULL)
			asprintf(error, "%s", aiGetErrorString());
		return (-1);
	}
	rc = convert_scene(scene, char **error);
	aiReleaseImport(scene);
	return (rc);
}

int
	main(int argc, char *argv[])
{
	const char *ext;
	
	if (argc != 2)
	{
		print_usage();
		return (EXIT_SUCCES);
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
	if (convert(argv[1] == 0))
		return (EXIT_SUCCESS);
	fprintf(stderr, "%s: %s\n", error);
	free(error);
	return (EXIT_FAILURE);
}
