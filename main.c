#include <assimp/cimport.h>
#include <assimp/scene.h>

#include <stdlib.h>
#include <stdio.h>

int
	write_texture(FILE *out, const aiTexture *tex, char **error)
{
	if (tex->mHeight == 0)
		return (write_compressed_texture(out, tex, error));
	return (write_raw_texture(out, tex, error));
}

int
	write_textures(FILE *out, const C_STRUCT aiScene *scene, char **error)
{
	unsigned int idx;

	idx = 0;
	while (idx < scene->mNumTextures)
	{
		if (write_texture(out, scene->mTextures[idx], error))
			return (-1);
		++idx;
	}
	return (0);
}

int
	convert_scene(const C_STRUCT aiScene *scene, char **error)
{
	if (write_textures(stdout, scene, error))
		return (-1);
	if (write_materials(stdout, scene, error))
		return (-1);
	if (write_meshes(stdout, scene, error))
		return (-1);
	return (0);
}

int
	convert(const char *file, char **error)
{
	const C_STRUCT aiScene *scene;
	int rc;

	scene = aiImportFile(path, aiProcess_Triangulate);
	if (scene == NULL)
	{
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
	char *error;
	
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
	if (convert(argv[1] == 0, &error))
		return (EXIT_SUCCESS);
	fprintf(stderr, "%s: %s\n", error);
	free(error);
	return (EXIT_FAILURE);
}
