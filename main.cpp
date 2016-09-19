#include <stdio.h>

#include <assimp/Importer.hpp> // C++ importer interface
#include <assimp/Exporter.hpp>
#include <assimp/scene.h> // Output data structure
#include <assimp/postprocess.h> // Post processing flags

typedef unsigned int uint;

struct ProgOpts {
  bool printFormats = false;
  const char * outFile = nullptr;
  const char * inFile = nullptr;

};

// void printMeshStates(const aiMesh* mesh, bool doBBox) {
//
// }
//
// void printSceneStats(const aiScene* scene) {
//
// }

ProgOpts readOpts(int argc, char** argv) {
  int c;
  ProgOpts out;
  while ((c = getopt(argc, argv, "xo:")) != -1) {
    switch (c) {
    case 'o':
      out.outFile = optarg;
      break;
    case 'x':
      out.printFormats = true;
      break;
      if (optopt == 'o')
        fprintf (stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint (optopt))
        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf (stderr,
          "Unknown option character `\\x%x'.\n",
          optopt);
    default:
        abort();
    }
  }
  for (int i = optind; i != argc; i++) {
    out.inFile = argv[i];
    break;
  }
  return out;
}



int main(int argc, char** argv) {
    ProgOpts opts = readOpts(argc, argv);
    Assimp::Importer importer;
    Assimp::Exporter::Exporter exporter;

    if (opts.printFormats) {
      size_t exportFormatCount = exporter.GetExportFormatCount();
      printf("There are %zu export formats available\n", exportFormatCount);
      for (size_t i = 0; i < exportFormatCount; i++) {
        const aiExportFormatDesc* pDesc = exporter.GetExportFormatDescription(i);
        printf("%zu - %s (.%s)\n", i, pDesc->description, pDesc->fileExtension);
      }
      return 0;
    }

    if (!opts.inFile) {
      fprintf(stderr, "No input file specified\n");
      return 1;
    }
    const std::string& pFile(opts.inFile);
    const aiScene* scene = importer.ReadFile( pFile, 0 );
    // If the import failed, report it
    if( !scene)
    {
        printf("%s\n", importer.GetErrorString());
        return 1;
    }

    printf("There are %d meshes in the scene\n", scene->mNumMeshes);
    for (size_t i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        printf("Mesh %zu has %d faces\n", i, mesh->mNumFaces);
    }

    if (opts.outFile) {
      const aiExportFormatDesc* pDesc = exporter.GetExportFormatDescription(3);
      exporter.Export(scene, pDesc->id, opts.outFile);
      printf("Exported to %s\n", opts.outFile);
    }

    return 0;
}
