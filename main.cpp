#include <stdio.h>
#include <limits>
#include <algorithm>

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

static const double FLOAT_MAX = std::numeric_limits<float>::max();
static const double FLOAT_MIN = std::numeric_limits<float>::min();

struct BBox {
  float minX = FLOAT_MAX;
  float maxX = FLOAT_MIN;

  float minY = FLOAT_MAX;
  float maxY = FLOAT_MIN;

  float minZ = FLOAT_MAX;
  float maxZ = FLOAT_MIN;
};

// void printMeshStats(const aiMesh* mesh, bool doBBox) {
//
// }
//

float calculateFaceVolume(const aiVector3D& a, const aiVector3D& b, const aiVector3D& c) {
  return (
    a.x*b.y*c.z +
    a.y*b.z*c.x +
    a.z*b.x*c.y -
    a.x*b.z*c.y -
    a.y*b.x*c.z -
    a.z*b.y*c.x) / 6;
}

float calculateMeshVolume(const aiMesh* pMesh) {
  float volume = 0;
  for (size_t i = 0; i < pMesh->mNumFaces; i++) {
    const aiFace& face = pMesh->mFaces[i];
    if (face.mNumIndices != 3) {
      printf("Encountered non-triangle face\n");
      abort();
    }
    volume += calculateFaceVolume(
      pMesh->mVertices[face.mIndices[0]],
      pMesh->mVertices[face.mIndices[1]],
      pMesh->mVertices[face.mIndices[2]]
    );
  }
  return volume;
}


BBox calculateBBox(const aiMesh* mesh) {
    BBox out;
    const size_t numVerts = mesh->mNumVertices;
    for (size_t i = 0; i < numVerts; i++) {
      const aiVector3D& v = mesh->mVertices[i];
      out.minX = std::min(out.minX, v.x);
      out.maxX = std::max(out.maxX, v.x);
      out.minY = std::min(out.minY, v.y);
      out.maxY = std::max(out.maxY, v.y);
      out.minZ = std::min(out.minZ, v.z);
      out.maxZ = std::max(out.maxZ, v.z);
    }
    return out;
}

void printSceneStats(const aiScene* scene) {
  printf("There are %d meshes in the scene\n", scene->mNumMeshes);
  for (size_t i = 0; i < scene->mNumMeshes; i++) {
      const aiMesh* mesh = scene->mMeshes[i];
      printf("Mesh %zu has %d faces\n", i, mesh->mNumFaces);
      BBox bb = calculateBBox(mesh);
      printf("BBox (%f, %f, %f)  (%f, %f, %f)\n", bb.minX, bb.minY, bb.minZ, bb.maxX, bb.maxY, bb.maxZ);
      printf("X %f\n", bb.maxX - bb.minX);
      printf("Y %f\n", bb.maxY - bb.minY);
      printf("Z %f\n", bb.maxZ - bb.minZ);
      float volume = calculateMeshVolume(mesh);
      printf("Volume %f\n", volume);
  }
}

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

void printFormats(const Assimp::Exporter& exporter) {
  size_t exportFormatCount = exporter.GetExportFormatCount();
  printf("There are %zu export formats available\n", exportFormatCount);
  for (size_t i = 0; i < exportFormatCount; i++) {
    const aiExportFormatDesc* pDesc = exporter.GetExportFormatDescription(i);
    printf("%zu - %s (.%s)\n", i, pDesc->description, pDesc->fileExtension);
  }
}

int main(int argc, char** argv) {
    ProgOpts opts = readOpts(argc, argv);
    Assimp::Importer importer;
    Assimp::Exporter::Exporter exporter;

    if (opts.printFormats) {
      printFormats(exporter);
      return 0;
    }
    if (!opts.inFile) {
      fprintf(stderr, "No input file specified\n");
      return 1;
    }

    // Read file
    const std::string& pFile(opts.inFile);
    //TODO: aiProcess::Triangulate
    const aiScene* scene = importer.ReadFile( pFile, 0 );
    if (!scene) {
        printf("%s\n", importer.GetErrorString());
        return 1;
    }

    printSceneStats(scene);

    // Export if outfile specified
    if (opts.outFile) {
      const aiExportFormatDesc* pDesc = exporter.GetExportFormatDescription(3);
      exporter.Export(scene, pDesc->id, opts.outFile);
      printf("Exported to %s\n", opts.outFile);
    }

    return 0;
}
