#include "stdafx.h"

#include "AnmImporter.h"

void AnmImporter::readFile(const std::string& path)
{
	ifstream inFile(path, ios::in | ios::binary);
	if (!inFile.is_open())
	{
		throw lol2daeError("Unable to open animation file.");
	}

	char magicNumber[8];
	inFile.read(magicNumber, 8);

	if (memcmp(magicNumber, "r3d2anmd", 8) != 0 && memcmp(magicNumber, "r3d2canm", 8) != 0)
	{
		throw  lol2daeError("Animation file is invalid or unsupported.");
	}

	int version;
	inFile.read(reinterpret_cast<char*>(&version), 4);

	if (version > 5)
	{
		throw  lol2daeError("Animation version is currently unsupported.");
	}

	if (version == 1)
	{
		int fileSize;
		inFile.read(reinterpret_cast<char*>(&fileSize), 4);
		fileSize += 12;
		inFile.seekg(8, ios_base::cur);

		inFile.read(reinterpret_cast<char*>(&numBones), 4);
		int numEntries;
		inFile.read(reinterpret_cast<char*>(&numEntries), 4);
		inFile.seekg(4, ios_base::cur);

		float animationLength;
		inFile.read(reinterpret_cast<char*>(&animationLength), 4);
		float framesPerSecond;
		inFile.read(reinterpret_cast<char*>(&framesPerSecond), 4);
		numFrames = (int)(animationLength * framesPerSecond);
		frameDelay = 1.0f / framesPerSecond;

		inFile.seekg(24, ios_base::cur);

		Vec3<float> minTranslation;
		inFile.read(reinterpret_cast<char*>(&minTranslation[0]), 4);
		inFile.read(reinterpret_cast<char*>(&minTranslation[1]), 4);
		inFile.read(reinterpret_cast<char*>(&minTranslation[2]), 4);

		Vec3<float> maxTranslation;
		inFile.read(reinterpret_cast<char*>(&maxTranslation[0]), 4);
		inFile.read(reinterpret_cast<char*>(&maxTranslation[1]), 4);
		inFile.read(reinterpret_cast<char*>(&maxTranslation[2]), 4);

		Vec3<float> minScale;
		inFile.read(reinterpret_cast<char*>(&minScale[0]), 4);
		inFile.read(reinterpret_cast<char*>(&minScale[1]), 4);
		inFile.read(reinterpret_cast<char*>(&minScale[2]), 4);

		Vec3<float> maxScale;
		inFile.read(reinterpret_cast<char*>(&maxScale[0]), 4);
		inFile.read(reinterpret_cast<char*>(&maxScale[1]), 4);
		inFile.read(reinterpret_cast<char*>(&maxScale[2]), 4);

		int entriesOffset, indicesOffset, hashesOffset;
		inFile.read(reinterpret_cast<char*>(&entriesOffset), 4);
		inFile.read(reinterpret_cast<char*>(&indicesOffset), 4);
		inFile.read(reinterpret_cast<char*>(&hashesOffset), 4);

		entriesOffset += 12;
		indicesOffset += 12;
		hashesOffset += 12;

		// const int hashBlock = 4;
		vector<unsigned int> hashEntries;

		inFile.seekg(hashesOffset, ios_base::beg);

		for (int i = 0; i < numBones; ++i)
		{
			unsigned int hashEntry;
			inFile.read(reinterpret_cast<char*>(&hashEntry), 4);
			hashEntries.push_back(hashEntry);
		}

		inFile.seekg(entriesOffset, ios_base::beg);

		const unsigned char quaternionType = 0;
		const unsigned char translationType = 64;
		const unsigned char scaleType = 128;

		vector<vector<pair <unsigned short, bitset<48>>>> compressedQuaternions, compressedTranslations, compressedScales;
		compressedQuaternions.resize(numBones);
		compressedTranslations.resize(numBones);
		compressedScales.resize(numBones);

		for (int i = 0; i < numEntries; ++i)
		{
			unsigned short compressedTime;
			inFile.read(reinterpret_cast<char*>(&compressedTime), 2);

			unsigned char hashIndex;
			inFile.read(reinterpret_cast<char*>(&hashIndex), 1);

			unsigned char dataType;
			inFile.read(reinterpret_cast<char*>(&dataType), 1);

			bitset<48> compressedData;
			inFile.read(reinterpret_cast<char*>(&compressedData), 6);

			// int boneHash = hashEntries.at(hashIndex);

			if (dataType == quaternionType)
			{
				compressedQuaternions.at(hashIndex).push_back(pair<unsigned short, bitset<48>>(compressedTime, compressedData));
			}

			else if (dataType == translationType)
			{
				compressedTranslations.at(hashIndex).push_back(pair<unsigned short, bitset<48>>(compressedTime, compressedData));
			}

			else if (dataType == scaleType)
			{
				compressedScales.at(hashIndex).push_back(pair<unsigned short, bitset<48>>(compressedTime, compressedData));
			}
		}

		for (int i = 0; i < numBones; ++i)
		{
			unsigned int boneHash = hashEntries.at(i);

			if (boneHashes.find(boneHash) == boneHashes.end())
			{
				continue;
			}

			AnmBone boneEntry;
			strcpy(boneEntry.name, boneHashes.at(boneHash));

			unordered_set<short> translationsTimeSet;

			for (auto &t : compressedTranslations.at(i))
			{
				auto res = translationsTimeSet.insert(t.first);

				if (!res.second)
				{
					continue;
				}

				float uncompressedTime = uncompressTime(t.first, animationLength);

				bitset<48> mask = 0xFFFF;
				unsigned short sx = static_cast<unsigned short>((t.second & mask).to_ulong());
				unsigned short sy = static_cast<unsigned short>((t.second >> 16 & mask).to_ulong());
				unsigned short sz = static_cast<unsigned short>((t.second >> 32 & mask).to_ulong());

				Vec3<float> translationEntry = uncompressVector(minTranslation, maxTranslation, sx, sy, sz);

				boneEntry.translation.push_back(pair<float, Vec3<float>>(uncompressedTime, translationEntry));
			}

			unordered_set<short> rotationsTimeSet;

			for (auto &r : compressedQuaternions.at(i))
			{

				auto res = rotationsTimeSet.insert(r.first);

				if (!res.second)
				{
					continue;
				}

				float uncompressedTime = uncompressTime(r.first, animationLength);

				bitset<48> mask = 0x7FFF;
				unsigned short flag = static_cast<unsigned short>((r.second >> 45).to_ulong());
				unsigned short sx = static_cast<unsigned short>((r.second >> 30 & mask).to_ulong());
				unsigned short sy = static_cast<unsigned short>((r.second >> 15 & mask).to_ulong());
				unsigned short sz = static_cast<unsigned short>((r.second & mask).to_ulong());

				Quat<float> quaterionEntry = uncompressQuaternion(flag, sx, sy, sz);

				boneEntry.quaternion.push_back(pair<float, Quat<float>>(uncompressedTime, quaterionEntry));
			}

			unordered_set<short> scaleTimeSet;

			for (auto &s : compressedScales.at(i))
			{

				auto res = scaleTimeSet.insert(s.first);

				if (!res.second)
				{
					continue;
				}

				float uncompressedTime = uncompressTime(s.first, animationLength);

				bitset<48> mask = 0xFFFF;
				unsigned short sx = static_cast<unsigned short>((s.second & mask).to_ulong());
				unsigned short sy = static_cast<unsigned short>((s.second >> 16 & mask).to_ulong());
				unsigned short sz = static_cast<unsigned short>((s.second >> 32 & mask).to_ulong());

				Vec3<float> scaleEntry = uncompressVector(minScale, maxScale, sx, sy, sz);

				boneEntry.scale.push_back(pair<float, Vec3<float>>(uncompressedTime, scaleEntry));
			}

			bones.push_back(boneEntry);
		}
	}

	else if (version == 3)
	{
		inFile.seekg(4, ios_base::cur);
		inFile.read(reinterpret_cast<char*>(&numBones), 4);
		inFile.read(reinterpret_cast<char*>(&numFrames), 4);
		int framesPerSecond;
		inFile.read(reinterpret_cast<char*>(&framesPerSecond), 4);
		frameDelay = 1.0f / framesPerSecond;

		bones.resize(numBones);

		for (int i = 0; i < numBones; ++i)
		{
			char name[32];
			inFile.read(name, 32);

			unsigned int boneHash = StringToHash(name);

			if (boneHashes.find(boneHash) != boneHashes.end())
			{
				strcpy(bones.at(i).name, boneHashes.at(boneHash));
			}

			else
			{
				strcpy(bones.at(i).name, name);
			}

			inFile.seekg(4, ios_base::cur);

			bones.at(i).translation.resize(numFrames);
			bones.at(i).quaternion.resize(numFrames);
			bones.at(i).scale.resize(numFrames);

			float cumulativeFrameDelay = 0.0f;

			for (int j = 0; j < numFrames; ++j)
			{
				bones.at(i).quaternion.at(j).first = cumulativeFrameDelay;
				bones.at(i).translation.at(j).first = cumulativeFrameDelay;
				bones.at(i).scale.at(j).first = cumulativeFrameDelay;
				inFile.read(reinterpret_cast<char*>(&bones.at(i).quaternion.at(j).second.v.x), 4);
				inFile.read(reinterpret_cast<char*>(&bones.at(i).quaternion.at(j).second.v.y), 4);
				inFile.read(reinterpret_cast<char*>(&bones.at(i).quaternion.at(j).second.v.z), 4);
				inFile.read(reinterpret_cast<char*>(&bones.at(i).quaternion.at(j).second.r), 4);
				inFile.read(reinterpret_cast<char*>(&bones.at(i).translation.at(j).second), 12);
				bones.at(i).scale.at(j).second[0] = bones.at(i).scale.at(j).second[1] = bones.at(i).scale.at(j).second[2] = 1.0f;
				cumulativeFrameDelay += frameDelay;
			}
		}
	}

	else if (version == 4)
	{
		inFile.seekg(16, ios_base::cur);
		inFile.read(reinterpret_cast<char*>(&numBones), 4);
		inFile.read(reinterpret_cast<char*>(&numFrames), 4);
		inFile.read(reinterpret_cast<char*>(&frameDelay), 4);
		inFile.seekg(12, ios_base::cur);

		int translationsOffset, quaternionsOffset, framesOffset;
		inFile.read(reinterpret_cast<char*>(&translationsOffset), 4);
		translationsOffset += 12;
		inFile.read(reinterpret_cast<char*>(&quaternionsOffset), 4);
		quaternionsOffset += 12;
		inFile.read(reinterpret_cast<char*>(&framesOffset), 4);
		framesOffset += 12;

		const int positionBlockSize = 12;
		vector<Vec3<float>> translationEntries;

		int numTranslationEntries = (int)(quaternionsOffset - translationsOffset) / positionBlockSize;

		inFile.seekg(translationsOffset, ios_base::beg);

		for (int i = 0; i < numTranslationEntries; ++i)
		{
			Vec3<float> translationEntry;
			inFile.read(reinterpret_cast<char*>(&translationEntry), 12);
			translationEntries.push_back(translationEntry);
		}

		const int quaternionBlockSize = 16;
		vector<Quat<float>> quaternionEntries;

		int numQuaternionEntries = (int)(framesOffset - quaternionsOffset) / quaternionBlockSize;

		inFile.seekg(quaternionsOffset, ios_base::beg);

		for (int i = 0; i < numQuaternionEntries; ++i)
		{
			Quat<float> quaternionEntry;
			inFile.read(reinterpret_cast<char*>(&quaternionEntry.v.x), 4);
			inFile.read(reinterpret_cast<char*>(&quaternionEntry.v.y), 4);
			inFile.read(reinterpret_cast<char*>(&quaternionEntry.v.z), 4);
			inFile.read(reinterpret_cast<char*>(&quaternionEntry.r), 4);
			quaternionEntries.push_back(quaternionEntry);
		}

		struct FrameIndices
		{
			short translationIndex;
			short quaternionIndex;
			short scaleIndex;
		};

		map<unsigned int, vector<FrameIndices>> boneMap;

		inFile.seekg(framesOffset, ios_base::beg);

		for (int i = 0; i < numBones; ++i)
		{
			for (int j = 0; j < numFrames; ++j)
			{
				unsigned int boneHash; 
				FrameIndices fi;

				inFile.read(reinterpret_cast<char*>(&boneHash), 4);
				inFile.read(reinterpret_cast<char*>(&fi.translationIndex), 2);
				inFile.read(reinterpret_cast<char*>(&fi.scaleIndex), 2);
				inFile.read(reinterpret_cast<char*>(&fi.quaternionIndex), 2);
				inFile.seekg(2, ios_base::cur);

				boneMap[boneHash].push_back(fi);
			}
		}

		for (auto& i : boneMap)
		{
			float cumulativeFrameDelay = 0.0f;

			if (boneHashes.find(i.first) == boneHashes.end())
			{
				continue;
			}

			AnmBone boneEntry;
			strcpy(boneEntry.name, boneHashes.at(i.first));

			for (auto& f : i.second)
			{			
				boneEntry.translation.push_back(pair<float, Vec3<float>>(cumulativeFrameDelay, translationEntries.at(f.translationIndex)));			
				boneEntry.quaternion.push_back(pair<float, Quat<float>>(cumulativeFrameDelay, quaternionEntries.at(f.quaternionIndex)));
				boneEntry.scale.push_back(pair<float, Vec3<float>>(cumulativeFrameDelay, translationEntries.at(f.scaleIndex)));
				cumulativeFrameDelay += frameDelay;
			}

			bones.push_back(boneEntry);
		}
	}

	else if (version == 5)
	{
		int fileSize;
		inFile.read(reinterpret_cast<char*>(&fileSize), 4);
		fileSize += 12;

		inFile.seekg(12, ios_base::cur);

		inFile.read(reinterpret_cast<char*>(&numBones), 4);
		inFile.read(reinterpret_cast<char*>(&numFrames), 4);
		inFile.read(reinterpret_cast<char*>(&frameDelay), 4);

		int translationsOffset, quaternionsOffset, framesOffset, hashesOffset;

		inFile.read(reinterpret_cast<char*>(&hashesOffset), 4);
		inFile.seekg(8, ios_base::cur);
		inFile.read(reinterpret_cast<char*>(&translationsOffset), 4);
		inFile.read(reinterpret_cast<char*>(&quaternionsOffset), 4);
		inFile.read(reinterpret_cast<char*>(&framesOffset), 4);

		translationsOffset += 12;
		quaternionsOffset += 12;
		framesOffset += 12;
		hashesOffset += 12;

		const int translationBlock = 12;
		vector<Vec3<float>> translationEntries;

		int numTranslationEntries = (int)(quaternionsOffset - translationsOffset) / translationBlock;

		inFile.seekg(translationsOffset, ios_base::beg);

		for (int i = 0; i < numTranslationEntries; ++i)
		{
			Vec3<float> translationEntry;
			inFile.read(reinterpret_cast<char*>(&translationEntry), 12);
			translationEntries.push_back(translationEntry);
		}

		const int quaternionBlock = 6;
		vector<bitset<48>> quaternionEntries;

		int numQuaternionEntries = (int)(hashesOffset - quaternionsOffset) / quaternionBlock;

		inFile.seekg(quaternionsOffset, ios_base::beg);

		for (int i = 0; i < numQuaternionEntries; ++i)
		{
			bitset<48> quaternionEntry;
			inFile.read(reinterpret_cast<char*>(&quaternionEntry), 6);
			quaternionEntries.push_back(quaternionEntry);
		}

		const int hashBlock = 4;
		vector<unsigned int> hashEntries;

		int numHashEntries = (int)(framesOffset - hashesOffset) / hashBlock;

		inFile.seekg(hashesOffset, ios_base::beg);

		for (int i = 0; i < numHashEntries; ++i)
		{
			unsigned int hashEntry;
			inFile.read(reinterpret_cast<char*>(&hashEntry), 4);
			hashEntries.push_back(hashEntry);
		}
		
		bones.resize(numBones);

		inFile.seekg(framesOffset, ios_base::beg);

		float cumulativeFrameDelay = 0.0f;

		for (int i = 0; i < numFrames; ++i)
		{
			for (int j = 0; j < numBones; ++j)
			{
				if (boneHashes.find(hashEntries.at(j)) != boneHashes.end())
				{
					strcpy(bones.at(j).name, boneHashes.at(hashEntries.at(j)));
				}

				short translationIndex, quaternionIndex, scaleIndex;
				inFile.read(reinterpret_cast<char*>(&translationIndex), 2);
				inFile.read(reinterpret_cast<char*>(&scaleIndex), 2);
				inFile.read(reinterpret_cast<char*>(&quaternionIndex), 2);
			
				bones.at(j).translation.push_back(pair<float, Vec3<float>>(cumulativeFrameDelay, translationEntries.at(translationIndex)));
				
				bitset<48> mask = 0x7FFF;
				unsigned short flag = static_cast<unsigned short>((quaternionEntries.at(quaternionIndex) >> 45).to_ulong());
				unsigned short sx = static_cast<unsigned short>((quaternionEntries.at(quaternionIndex) >> 30 & mask).to_ulong());
				unsigned short sy = static_cast<unsigned short>((quaternionEntries.at(quaternionIndex) >> 15 & mask).to_ulong());
				unsigned short sz = static_cast<unsigned short>((quaternionEntries.at(quaternionIndex) & mask).to_ulong());

				Quat<float> quaterionEntry = uncompressQuaternion(flag, sx, sy, sz);
				
				bones.at(j).quaternion.push_back(pair<float, Quat<float>>(cumulativeFrameDelay, quaterionEntry));

				bones.at(j).scale.push_back(pair<float, Vec3<float>>(cumulativeFrameDelay, translationEntries.at(scaleIndex)));
			}

			cumulativeFrameDelay += frameDelay;
		}

		auto it = bones.begin();

		while (it != bones.end())
		{
			if (!isalpha(it->name[0]))
			{
				it = bones.erase(it);
			}

			else
			{
				++it;
			}
		}
	}
}

Quat<float> AnmImporter::uncompressQuaternion(const unsigned short& flag, const unsigned short& sx, const unsigned short& sy, const unsigned short& sz)
{
	float fx = sqrt(2.0f) * ((int)sx - 16384) / 32768.0f;
	float fy = sqrt(2.0f) * ((int)sy - 16384) / 32768.0f;
	float fz = sqrt(2.0f) * ((int)sz - 16384) / 32768.0f;
	float fw = sqrt(1.0f - fx * fx - fy * fy - fz * fz);

	Quat<float> uq;

	switch (flag)
	{
	case 0:
		uq.v.x = fw;
		uq.v.y = fx;
		uq.v.z = fy;
		uq.r = fz;
		break;

	case 1:
		uq.v.x = fx;
		uq.v.y = fw;
		uq.v.z = fy;
		uq.r = fz;
		break;

	case 2:
		uq.v.x = -fx;
		uq.v.y = -fy;
		uq.v.z = -fw;
		uq.r = -fz;
		break;

	case 3:
		uq.v.x = fx;
		uq.v.y = fy;
		uq.v.z = fz;
		uq.r = fw;
		break;
	}

	return uq;
}

Vec3<float> AnmImporter::uncompressVector(const Vec3<float>& min, const Vec3<float>& max, const unsigned short& sx, const unsigned short& sy, const unsigned short& sz)
{
	Vec3<float> uv;

	uv = max - min;

	uv.x *= (sx / 65535.0f);
	uv.y *= (sy / 65535.0f);
	uv.z *= (sz / 65535.0f);

	uv = uv + min;

	return uv;
}

float AnmImporter::uncompressTime(const unsigned short& ct, const float& animationLength)
{
	float ut;

	ut = ct / 65535.0f;
	ut = ut * animationLength;

	return ut;
}

unsigned int AnmImporter::StringToHash(string s)
{
	unsigned int hash = 0;
	unsigned int temp = 0;

	for (auto& c : s)
	{
		hash = (hash << 4) + tolower(c);
		temp = hash & 0xf0000000;

		if (temp != 0)
		{
			hash = hash ^ (temp >> 24);
			hash = hash ^ temp;
		}
	}

	return hash;
}

AnmImporter::~AnmImporter()
{
}
