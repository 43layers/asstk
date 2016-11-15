#include <stdio.h>
#include <unistd.h>
#include <limits>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <list>

#include <assimp/Importer.hpp> // C++ importer interface
#include <assimp/Exporter.hpp>
#include <assimp/Logger.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/scene.h> // Output data structure
#include <assimp/postprocess.h> // Post processing flags

#include <Magick++.h>

#include "boost/filesystem.hpp"
using boost::filesystem::path;

#define println(x) std::cout << x << std::endl;


typedef unsigned int uint;

struct ProgOpts {
  bool printFormats = false;
  const char * outFile = nullptr;
  const char * inFile = nullptr;
  double scale = 1.0;
  int outFormat = -1;
  bool printStats = false;
  bool combineMeshes = false;
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
  while ((c = getopt(argc, argv, "s:xtco:f:")) != -1) {
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
    case 't':
      out.printStats = true;
      break;
    case 'c':
      out.combineMeshes = true;
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

aiScene combineMeshes(uint numMeshes, aiMesh** pMeshes, path useTexture) {
  aiMesh* pCombined = nullptr;
  aiScene scene;
  {
    scene.mRootNode = new aiNode();
    scene.mMaterials = new aiMaterial*[1];
    scene.mMaterials[0] = new aiMaterial();
    scene.mNumMaterials = 1;

    aiString texPath(useTexture.string());
    scene.mMaterials[0]->AddProperty(&texPath, AI_MATKEY_TEXTURE_DIFFUSE(0));

    scene.mNumMeshes = 1;
    scene.mMeshes = new aiMesh*[1];
    scene.mMeshes[0] = new aiMesh();
    pCombined = scene.mMeshes[0];
    pCombined->mMaterialIndex = 0;

    scene.mRootNode->mMeshes = new unsigned int[1];
    scene.mRootNode->mMeshes[0] = 0;
    scene.mRootNode->mNumMeshes = 1;
  }

  pCombined->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

  // Make first pass to collect the total numbers of members
  for (uint i = 0; i < numMeshes; i++) {
    aiMesh* pMesh = pMeshes[i];
    pCombined->mNumFaces += pMesh->mNumFaces;
    pCombined->mNumVertices += pMesh->mNumVertices;
  }

  pCombined->mFaces = new aiFace[pCombined->mNumFaces];
  pCombined->mVertices = new aiVector3D[pCombined->mNumVertices];
  pCombined->mNumUVComponents[0] = 2;
  pCombined->mTextureCoords[0] = new aiVector3D[pCombined->mNumVertices];

  // Second pass to fill created arrays
  size_t faceOffset = 0;
  size_t vertexOffset = 0;
  for (uint i = 0; i < numMeshes; i++) {
    aiMesh* pMesh = pMeshes[i];

    for(uint tci = 0; tci < pMesh->mNumVertices; tci++) {
      const auto& v = pMesh->mVertices[tci];
      pCombined->mVertices[vertexOffset + tci] = aiVector3D(v.x, v.y, v.z);

      const auto& t = pMesh->mTextureCoords[0][tci];
      pCombined->mTextureCoords[0][vertexOffset + tci] = aiVector3D((t.x + i) / numMeshes, t.y, 0);
    }

    for (uint faceIdx = 0; faceIdx < pMesh->mNumFaces; faceIdx++) {
      aiFace f = pMesh->mFaces[faceIdx];
      for (uint idxIdx = 0; idxIdx < f.mNumIndices; idxIdx++) {
        f.mIndices[idxIdx] += vertexOffset;
      }
      pCombined->mFaces[faceIdx + faceOffset] = f;
    }

    faceOffset += pMesh->mNumFaces;
    vertexOffset += pMesh->mNumVertices;
  }

  return scene;
}

void convertImage(std::string inPath, std::string outPath) {
  Magick::Image img;
  img.read(inPath);
  img.write(outPath);
}

bool montageImages(std::vector<path> images, path writeTo) {
  std::list<Magick::Image> sourceImageList;
  Magick::Image image;

  for (path p : images) {
    if (!boost::filesystem::exists(p)) {
      println(p << " was not found on filesystem");
      return false;
    }
    if (!boost::filesystem::is_regular_file(p)) {
      println(p << " is not a regular file");
      return false;
    }

    image.read(p.string());
    sourceImageList.push_back(image);
  }

  Magick::Color color("rgba(0,0,0,0)");
  Magick::Montage montageSettings;
  montageSettings.geometry("4096x4096-0-0");
  montageSettings.shadow(false);
  montageSettings.backgroundColor(color);
  montageSettings.tile(Magick::Geometry(sourceImageList.size(), 1));

  std::list<Magick::Image> montageList;
  Magick::montageImages(&montageList, sourceImageList.begin(), sourceImageList.end(), montageSettings);
  Magick::writeImages(montageList.begin(), montageList.end(), writeTo.string());

  return true;
}

void convertSceneTextures(const aiScene* pScene, path inpath, path outpath) {
  path stem = outpath.stem();
  path inDir = inpath.parent_path();
  path outDir = outpath.parent_path();

  for (size_t meshIdx = 0; meshIdx < pScene->mNumMeshes; meshIdx++) {
    // Find mesh's existing texture
    const aiMesh* pMesh = pScene->mMeshes[meshIdx];
    if (!pMesh->HasTextureCoords(0)) continue;
    uint materialIndex = pMesh->mMaterialIndex;
    aiMaterial* pMaterial = pScene->mMaterials[materialIndex];
    aiString s;
    path oldTexturePath = inDir;
    if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), s)) {
      oldTexturePath /= path(s.data);
    }

    // Rename it (within the mesh) to NAME_tex_N.EXT
    path ext = oldTexturePath.extension();
    std::stringstream newTextureNameStream;
    newTextureNameStream << stem.string() << "_tex_" << meshIdx << ".jpg";
    aiString newName(newTextureNameStream.str());
    pMaterial->AddProperty(&newName, AI_MATKEY_TEXTURE_DIFFUSE(0));

    // Copy, convert, and rename the image file to its new home
    path texOut = outDir;
    texOut /= path(newTextureNameStream.str());
    path texIn = inDir;
    texIn /= path(oldTexturePath);
    convertImage(texIn.string(), texOut.string());
  }
}

path combineSceneTextures(const aiScene* pScene, path inpath, path outpath) {
    path stem = outpath.stem();
    path inDir = inpath.parent_path();
    path outDir = outpath.parent_path();

    std::vector<path> oldTextures;

    for (size_t meshIdx = 0; meshIdx < pScene->mNumMeshes; meshIdx++) {
      // Find mesh's existing texture
      const aiMesh* pMesh = pScene->mMeshes[meshIdx];
      if (!pMesh->HasTextureCoords(0)) continue;
      uint materialIndex = pMesh->mMaterialIndex;
      aiMaterial* pMaterial = pScene->mMaterials[materialIndex];
      aiString s;
      path oldTexturePath = inDir;
      if (AI_SUCCESS == pMaterial->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), s)) {
        oldTexturePath /= path(s.data);
      }
      oldTextures.push_back(oldTexturePath);
    }

    path montageOut = outDir;
    std::stringstream newTextureNameStream;
    newTextureNameStream << stem.string() << "_tex.jpg";
    montageOut /= path(newTextureNameStream.str());
    if (montageImages(oldTextures, montageOut)) {
      std::cout << "Combined textures successfully" << std::endl;
    } else {
      std::cout << "Combining textures failed" << std::endl;
    }
    return montageOut.filename();;
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

    if (opts.printStats) {
      printSceneStats(scene);
    }

    if (opts.scale != 1.0) {
      printf("Scaling mesh by %f\n", opts.scale);
      scaleSceneMeshes(scene, opts.scale);
    }

    // Mesh wrangling operations
    {
    }

    // Export if outfile specified
    if (opts.outFile) {
      path outFilePath(opts.outFile);
      std::string stem = outFilePath.stem().string();
      std::string ext = outFilePath.extension().string().substr(1);

      const aiExportFormatDesc* pOutDesc = nullptr;
      if (opts.outFormat != -1) {
        pOutDesc = exporter.GetExportFormatDescription(opts.outFormat);
        if (ext.compare(pOutDesc->fileExtension)) {
          outFilePath += pOutDesc->fileExtension;
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

      if (opts.combineMeshes) {
        path texOut = combineSceneTextures(scene, opts.inFile, opts.outFile);
        aiScene combinedScene = combineMeshes(scene->mNumMeshes, scene->mMeshes, texOut);
        exporter.Export(&combinedScene, pOutDesc->id, outFilePath.string());
      } else {
        convertSceneTextures(scene, opts.inFile, opts.outFile);
        exporter.Export(scene, pOutDesc->id, outFilePath.string());
      }
      std::cout << "Exported to " << outFilePath << std::endl;
    }

    Assimp::DefaultLogger::kill();
    return 0;
}
