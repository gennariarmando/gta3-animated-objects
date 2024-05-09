#include "plugin.h"
#include "CSimpleModelInfo.h"
#include "CModelInfo.h"
#include "SpriteLoader.h"

class AnimatedObjects {
public:
    struct Flipbook {
    public:
        int32_t currentFrame;
        int32_t currentTime;
        bool revert;

    public:
        inline Flipbook() {
            currentFrame = 0;
            currentTime = 0;
            revert = false;
        }

        bool Process(int32_t timeRate, int32_t lastFrame, uint8_t loopMode) {
            if (currentTime < CTimer::m_snTimeInMilliseconds) {
                currentTime = CTimer::m_snTimeInMilliseconds + timeRate;

                if (revert)
                    currentFrame--;
                else
                    currentFrame++;
            }

            switch (loopMode) {
                case 1:
                    if (currentFrame >= lastFrame) {
                        currentFrame = lastFrame;
                        revert = true;
                        return true;
                    }
                    else if (currentFrame <= 0) {
                        currentFrame = 0;
                        revert = false;
                        return true;
                    }
                    break;
                default:
                    if (currentFrame > lastFrame) {
                        currentFrame = 0;
                        return true;
                    }
                    break;
            }

            return false;
        }
    };

    struct AnimObject {
        std::string modelName;
        std::string firstTexName;
        std::string currentTexName; // initially same as firstTexName, then follows current frame name
        int32_t timeRate;
        int32_t numFrames;
        uint8_t loopMode;
        Flipbook* flipBook;
        int32_t modelId;
    };

    static inline std::vector<AnimObject> listOfObjects = {};
    static inline plugin::SpriteLoader spriteLoader = {};
    static inline bool initialised = false;

public:
    static void Process() {
        if (!initialised)
            return;

        for (auto& obj : listOfObjects) {
            if (obj.modelId != -1) {
                obj.flipBook->Process(obj.timeRate, obj.numFrames, obj.loopMode);

                CSimpleModelInfo* mi = (CSimpleModelInfo*)CModelInfo::GetModelInfo(obj.modelId);
                for (int32_t atomicId = 0; atomicId < mi->m_nNumAtomics; atomicId++) {
                    RpAtomic* atomic = mi->m_apAtomics[atomicId];

                    if (!atomic)
                        break;

                    for (int32_t i = 0; i < atomic->geometry->matList.numMaterials; i++) {
                        RpMaterial* mat = atomic->geometry->matList.materials[i];

                        if (!mat->texture || !strncmp(mat->texture->name, obj.currentTexName.c_str(), 32)) {
                            if (obj.flipBook->currentFrame > 0) {
                                char buf[32];
                                sprintf_s(buf, 32, "%s_%d", obj.firstTexName.c_str(), obj.flipBook->currentFrame);
                                mat->texture = spriteLoader.GetTex(buf);
                                obj.currentTexName = buf;
                            }
                            else {
                                mat->texture = spriteLoader.GetTex(obj.firstTexName);
                                obj.currentTexName = obj.firstTexName;
                            }
                        }
                    }
                }
            }
        }
    }

    static void ReadData() {
        std::ifstream file(PLUGIN_PATH("AnimatedObjectsIII.dat"));
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }

                std::istringstream iss(line);
                std::string token;
                std::vector<std::string> tokens;

                while (std::getline(iss, token, ',')) {
                    token.erase(std::remove_if(token.begin(), token.end(), [](char c) { return std::isspace(c); }), token.end());
                    tokens.push_back(token);
                }

                if (tokens.size() != 5)
                    continue;

                std::string modelName = tokens[0];
                std::string firstTexName = tokens[1];
                int32_t timeRate = std::stoi(tokens[2]);
                int32_t numFrames = std::stoi(tokens[3]);
                uint8_t loopMode = std::stoi(tokens[4]);

                AnimObject obj;
                obj.modelName = modelName;
                obj.firstTexName = firstTexName;
                obj.timeRate = timeRate;
                obj.numFrames = numFrames;
                obj.loopMode = loopMode;

                listOfObjects.push_back(obj);
            }

            file.close();
        }
    }

    static void Init() {
        if (initialised)
            return;

        ReadData();

        for (auto& obj : listOfObjects) {
            spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("AnimatedObjects/") + obj.modelName);
            obj.currentTexName = obj.firstTexName;
            obj.flipBook = new Flipbook();

            int32_t index = -1;
            auto* mi = CModelInfo::GetModelInfo(obj.modelName.c_str(), &index);
            obj.modelId = index;
        }

        initialised = true;
    }

    static void Shutdown() {
        if (!initialised)
            return;

        spriteLoader.Clear();
        spriteLoader = {};

        for (auto& it : listOfObjects) {
            if (it.flipBook)
                delete it.flipBook;
        }
        listOfObjects = {};

        initialised = false;
    }

    AnimatedObjects() {
        plugin::Events::initGameEvent += []() {
            Init();
        };

        plugin::Events::shutdownRwEvent += []() {
            Shutdown();
        };

        plugin::Events::processScriptsEvent += []() {
            Process();
        };

    }
} animatedObjects;
