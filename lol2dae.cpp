#include "stdafx.h"

#include "AnmImporter.h"
#include "SknImporter.h"
#include "SklImporter.h"
#include "ColladaWriter.h"


int main(int argc, char *argv[])
{
    if (argc != 4) {
	std::cerr << "Usage: " << argv[0] << " <skn> <anm> <output>" << std::endl;
	exit(1);
    }
    std::string sknPath = argv[1];
    std::string anmPath = argv[2];
    std::string outputPath = argv[3];

    // ColladaWriter::Mode mode = dlg->m_IncludeSkeleton.GetCheck() == 1) ? ColladaWriter::Mode::Skeleton : ColladaWriter::Mode::Mesh;
    ColladaWriter::Mode mode = ColladaWriter::Mode::Animation;

    try 
    {
	SknImporter inputSkn;
	inputSkn.readFile(sknPath);

	SklImporter inputSkl(inputSkn.fileVersion);
	AnmImporter inputAnm(inputSkl.boneHashes);

	if (mode >= ColladaWriter::Mode::Skeleton)
	{
	    std::string sklPath = sknPath;

	    sklPath.replace(sknPath.length() - 3, 3, "skl");

	    inputSkl.readFile(sklPath);

	    if (mode == ColladaWriter::Mode::Animation)
	    {
		inputAnm.readFile(anmPath);
	    }
	}

	ColladaWriter outputCollada(inputSkn.indices, inputSkn.vertices, inputSkl.bones, inputSkl.boneIndices, inputAnm);
	outputCollada.writeFile(outputPath, mode);
	return 0;
    }
    catch (lol2daeError& e)
    {
	std::cerr << "error: " << e.what() << std::endl;
    }
    return 1;

}

