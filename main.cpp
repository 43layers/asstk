#include <stdio.h>
#include <limits>
#include <algorithm>
#include <iostream>

#include <assimp/Importer.hpp> // C++ importer interface
#include <assimp/Exporter.hpp>
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h> // Output data structure
#include <assimp/postprocess.h> // Post processing flags

typedef unsigned int uint;

struct ProgOpts {
  bool printFormats = false;
  const char * outFile = nullptr;
  const char * inFile = nullptr;
  double scale = 1.0;
  int outFormat = -1;
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

class myStream : public Assimp::LogStream {
public:
  myStream() { }
  ~myStream() { }
  void write(const char* message) override {
    printf("%s\n", message);
  }
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

void printIndent(unsigned int depth) {
  for (unsigned int i = 0; i < depth; i++) {
    printf("  ");
  }
}

void printMeshStats(const aiMesh* pMesh, unsigned int depth) {
  BBox bb = calculateBBox(pMesh);
  float volume = calculateMeshVolume(pMesh);

  printIndent(depth); printf("Mesh - %s\n", pMesh->mName.C_Str());
  printIndent(depth + 1); printf("%d faces\n", pMesh->mNumFaces);
  printIndent(depth + 1); printf("%d vertices\n", pMesh->mNumVertices);
  printIndent(depth + 1); printf("BBox (%f, %f, %f)  (%f, %f, %f)\n", bb.minX, bb.minY, bb.minZ, bb.maxX, bb.maxY, bb.maxZ);
  printIndent(depth + 1); printf("X %f\n", bb.maxX - bb.minX);
  printIndent(depth + 1); printf("Y %f\n", bb.maxY - bb.minY);
  printIndent(depth + 1); printf("Z %f\n", bb.maxZ - bb.minZ);
  printIndent(depth + 1); printf("Volume %f (%f)\n", volume, volume / 1000);
  printIndent(depth + 1); printf("Color channels: %d\n", pMesh->GetNumColorChannels());
  printIndent(depth + 1); printf("UV channels: %d\n", pMesh->GetNumUVChannels());
}

void printNode(const aiNode* pNode, aiMesh** meshes, unsigned int depth) {
  printIndent(depth);
  printf("Node - %s: %d meshes, %d children\n", pNode->mName.C_Str(), pNode->mNumMeshes, pNode->mNumChildren);
  for (size_t i = 0; i < pNode->mNumMeshes; i++) {
    printMeshStats(meshes[pNode->mMeshes[i]], depth + 1);
  }
  for (size_t i = 0; i < pNode->mNumChildren; i++) {
    printNode(pNode->mChildren[i], meshes, depth + 1);
  }
}

void printSceneStats(const aiScene* scene) {
  printNode(scene->mRootNode, scene->mMeshes, 0);
}

ProgOpts readOpts(int argc, char** argv) {
  int c;
  ProgOpts out;
  while ((c = getopt(argc, argv, "s:xo:f:")) != -1) {
    switch (c) {
    case 'o':
      out.outFile = optarg;
      break;
    case 'x':
      out.printFormats = true;
      break;
    case 's':
      out.scale = atof(optarg);
      break;
    case 'f':
      out.outFormat = atoi(optarg);
      break;
    case '?':
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

void scaleSceneMeshes(const aiScene* pScene, double scale) {
  for (size_t meshIdx =0; meshIdx < pScene->mNumMeshes; meshIdx++) {
    const aiMesh* pMesh = pScene->mMeshes[meshIdx];
    const size_t numVerts = pMesh->mNumVertices;
    for (size_t vertIdx = 0; vertIdx < numVerts; vertIdx++) {
      aiVector3D& v = pMesh->mVertices[vertIdx];
      v *= float(scale);
    }
  }
}

std::string getFilenameExt(std::string filename) {
  std::string::size_type idx = filename.rfind('.');
  std::string ext;
  if (idx != std::string::npos) {
    ext = filename.substr(idx+1);
  }
  return ext;
}

const aiExportFormatDesc* findFormatDescForExt(const Assimp::Exporter& exporter, std::string ext) {
  size_t exportFormatCount = exporter.GetExportFormatCount();
  for (size_t i = 0; i < exportFormatCount; i++) {
    const aiExportFormatDesc* pDesc = exporter.GetExportFormatDescription(i);
    if (ext.compare(pDesc->fileExtension) == 0) {
      return pDesc;
    }
  }
  return nullptr;
}

int main(int argc, char** argv) {
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
    unsigned int severity = Assimp::Logger::Err | Assimp::Logger::Warn;
    // TODO: add -v option for verbose to enable debug & info messages
    // severity |= Assimp::Logger::Debugging | Assimp::Logger::Info;
    Assimp::DefaultLogger::get()->attachStream(new myStream(), severity);
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
    const aiScene* scene = importer.ReadFile( pFile,
      aiProcess_Triangulate |
      aiProcess_JoinIdenticalVertices |
      aiProcess_ValidateDataStructure
      //aiProcess_SortByPType
    );
    if (!scene) {
        printf("Importer error: %s\n", importer.GetErrorString());
        return 1;
    }

    printSceneStats(scene);

    if (opts.scale != 1.0) {
      printf("Scaling mesh by %f\n", opts.scale);
      scaleSceneMeshes(scene, opts.scale);
    }

    // Export if outfile specified
    if (opts.outFile) {
      std::string outFile(opts.outFile);
      std::string ext = getFilenameExt(outFile);
      const aiExportFormatDesc* pOutDesc = nullptr;
      if (opts.outFormat != -1) {
        pOutDesc = exporter.GetExportFormatDescription(opts.outFormat);
        if (ext.compare(pOutDesc->fileExtension)) {
          outFile += pOutDesc->fileExtension;
        }
      } else {
        if (ext.empty()) {
          printf("No export format specified\n");
          abort();
        }
        pOutDesc = findFormatDescForExt(exporter, ext);
      }
      if (!pOutDesc) {
        printf("Couldn't find appropriate exporter for extension %s\n", ext.c_str());
        abort();
      }
      exporter.Export(scene, pOutDesc->id, outFile);
      printf("Exported to %s\n", opts.outFile);
    }

    Assimp::DefaultLogger::kill();
    return 0;
}
