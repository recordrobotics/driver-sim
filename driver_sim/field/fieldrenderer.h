#pragma once

#include <bgfx/bgfx.h>

#include <bimg/decode.h>
#include <bx/error.h>
#include <bx/file.h>
#include <bx/pixelformat.h>

#include "texture.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>
#include <random>
#include <unordered_map>
#include <vector>

#include "mesh.h"
#include <future>

#include <blackboard_app/gui.h>
#include <blackboard_app/logger.h>
#include <blackboard_app/window.h>

#include "../discord.h"

#include <networktables/BooleanTopic.h>
#include <networktables/IntegerTopic.h>
#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>
#include <networktables/StringArrayTopic.h>
#include <networktables/StructArrayTopic.h>
#include <networktables/StructTopic.h>

#include <wpi/struct/Struct.h>

#include <frc/geometry/Pose3d.h>
#include <frc/geometry/struct/Pose3dStruct.h>

#if GAME_YEAR == 2026
#include "seasonspecific/rebuilt2026/fmsui.h"
#include "seasonspecific/rebuilt2026/hublights.h"
#endif

inline constexpr bx::Quaternion rotation3dToQuaternion(const frc::Rotation3d &rotation)
{
    const frc::Quaternion &frcQuat = rotation.GetQuaternion();
    return {static_cast<float>(frcQuat.X()), static_cast<float>(frcQuat.Y()),
            static_cast<float>(frcQuat.Z()), static_cast<float>(frcQuat.W())};
}

inline constexpr bx::Quaternion rotation3dToQuaternionInverse(const frc::Rotation3d &rotation)
{
    const frc::Quaternion frcQuat = rotation.GetQuaternion().Inverse();
    return {static_cast<float>(frcQuat.X()), static_cast<float>(frcQuat.Y()),
            static_cast<float>(frcQuat.Z()), static_cast<float>(frcQuat.W())};
}

struct Pose3dObject
{
    frc::Pose3d pose;
    int identity;

    Pose3dObject(const frc::Pose3d &p, int id) : pose(p), identity(id) {}

    Pose3dObject() : pose(frc::Pose3d()), identity(0) {}
};

namespace
{
    constexpr size_t kPoseOff = 0;
    constexpr size_t kIdentityOff = kPoseOff + wpi::GetStructSize<frc::Pose3d>();
} // namespace

template <> struct wpi::Struct<Pose3dObject>
{
    static constexpr std::string_view GetTypeName() { return "Pose3dObject"; }
    static constexpr size_t GetSize() { return wpi::GetStructSize<frc::Pose3d>() + 4; }
    static constexpr std::string_view GetSchema() { return "Pose3d pose;int identity"; }

    static Pose3dObject Unpack(std::span<const uint8_t> data)
    {
        return Pose3dObject{wpi::UnpackStruct<frc::Pose3d, kPoseOff>(data),
                            wpi::UnpackStruct<int, kIdentityOff>(data)};
    }
    static void Pack(std::span<uint8_t> data, const Pose3dObject &value)
    {
        wpi::PackStruct<kPoseOff>(data, value.pose);
        wpi::PackStruct<kIdentityOff>(data, value.identity);
    }
    static void ForEachNested(std::invocable<std::string_view, std::string_view> auto fn)
    {
        wpi::ForEachStructSchema<frc::Pose3d>(fn);
    }
};

static_assert(wpi::StructSerializable<Pose3dObject>);
static_assert(wpi::HasNestedStruct<Pose3dObject>);

enum class WPILibCoordinateSystem
{
    WallBlue,
    CenterRed
};

struct AprilTagInstanceData
{
    std::array<float, 16> modelMatrix{};

    float &id() { return modelMatrix[15]; }
    const float &id() const { return modelMatrix[15]; }
};

struct Transform
{
    // partial TRS matrix, row major
    float mRow0[4];
    float mRow1[4];
    float mRow2[4];

    Transform();
    Transform(float *modelMatrix, float *parentMatrix, bx::Vec3 position, bx::Quaternion rotation);

    void toMatrix(float *outMatrix) const;
};

struct InstanceData
{
    Transform transform;
    Transform previousTransform;
};

struct DynamicObjectData
{
    int lastDataUpdate = -1;
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    bx::Quaternion rotation = rotation3dToQuaternion(frc::Rotation3d());

    bx::Vec3 lastPosition = {0.0f, 0.0f, 0.0f};
    bx::Quaternion lastRotation = rotation3dToQuaternion(frc::Rotation3d());

    InstanceData instanceData = {};

    void update(float *modelMatrix, float *parentMatrix, float deltaTime,
                bool freezeTemporalEffects);

    void update(float *modelMatrix, float deltaTime, bool freezeTemporalEffects)
    {
        update(modelMatrix, nullptr, deltaTime, freezeTemporalEffects);
    }
};

struct RobotComponentData
{
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;
};

struct RobotModel
{
    std::string name;

    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;

    std::vector<RobotComponentData> components;

    float rslOnEmissionStrength = 0.0f;
    std::array<float, 4> bumperModelColor;
    std::optional<std::array<float, 4>> bumperBlueColor;
    std::optional<std::array<float, 4>> bumperRedColor;

    float ledCount = 0.0f;
    float ledAspectRatio = 1.0f;

    // NT-dependent materials use per-instance robot data
    // Meshes with these materials can't be instanced
    Material *rslMaterial = nullptr;
    Material *bumperMaterial = nullptr;
    Material *ledMaterial = nullptr;

    bool isInstanceable(const Material *material) const
    {
        return material != rslMaterial && material != bumperMaterial && material != ledMaterial;
    }

    // Construct, specifying only model matrices from config file
    RobotModel(std::string name, std::array<float, 16> modelMatrix,
               std::vector<RobotComponentData> components)
        : name(name), modelMatrix(modelMatrix), components(components)
    {
    }
};

struct RobotData
{
    RobotModel *model;

    DynamicObjectData dynamicData;
    std::vector<DynamicObjectData> components;

    nt::StructTopic<frc::Pose3d> poseTopic;
    nt::StructSubscriber<frc::Pose3d> poseSub;

    nt::StructArrayTopic<frc::Pose3d> componentPosesTopic;
    nt::StructArraySubscriber<frc::Pose3d> componentPosesSub;

    nt::BooleanTopic rslStateTopic;
    nt::BooleanSubscriber rslStateSub;

    nt::IntegerTopic allianceStationTopic;
    nt::IntegerSubscriber allianceStationSub;

    nt::StringArrayTopic ledColorsTopic;
    nt::StringArraySubscriber ledColorsSub;

    // is opponent robot enabled
    nt::BooleanTopic enabledTopic;
    nt::BooleanSubscriber enabledSub;
    bool alwaysEnabled = false;

    std::array<float, 4> bumperBaseColor;
    float rslEmissionStrength = 0.0f;

    // RGB8 texture
    Texture ledColorTexture;
    std::unique_ptr<uint8_t[]> ledColorData;

    RobotData(const RobotData &) = delete;
    RobotData &operator=(const RobotData &) = delete;
    RobotData(RobotData &&) noexcept = default;
    RobotData &operator=(RobotData &&) noexcept = default;

    RobotData(RobotModel *model);
    void update(
        int currentDataUpdateIndex, float deltaTime, bool freezeTemporalEffects,
        const std::function<frc::Pose3d(const frc::Pose3d &)> &transformPose3dToLocalCoordinates);
};

struct GamePieceData
{
    std::string name;
    std::unordered_map<int, DynamicObjectData> instances;
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;

    nt::StructArrayTopic<frc::Pose3d> posesTopic;
    nt::StructArraySubscriber<frc::Pose3d> posesSub;

    nt::StructArrayTopic<Pose3dObject> poseObjectsTopic;
    nt::StructArraySubscriber<Pose3dObject> poseObjectsSub;

    void update(int currentDataUpdateIndex, float deltaTime, bool freezeTemporalEffects,
                std::function<frc::Pose3d(const frc::Pose3d &)> transformPose3dToLocalCoordinates);
};

struct OrbitCamera
{
    bx::Vec3 target{0.0f, 0.0f, 0.25f};
    float distance = 15.811388f;
    float yaw = bx::toRad(90.0f);
    float pitch = bx::toRad(18.0f);
    float minDistance = 0.1f;
    float maxDistance = 120.0f;
    float orbitSpeed = 0.008f;

    float originTransform[16];

    OrbitCamera() { bx::mtxIdentity(originTransform); }

    bx::Vec3 getEye() const
    {
        const float cosPitch = std::cos(pitch);
        const float sinPitch = std::sin(pitch);
        const float sinYaw = std::sin(yaw);
        const float cosYaw = std::cos(yaw);

        return {target.x + distance * cosPitch * cosYaw, target.y - distance * cosPitch * sinYaw,
                target.z + distance * sinPitch};
    }
};

enum class DebugView
{
    None,
    Albedo,
    Normal,
    Emission,
    PBRData,
    Velocity,
    Depth,
    OITMoments,
    OITTotalDepth,
    OITAccum,
    MotionBlurVelocity,
    MotionBlurTileMaxX,
    MotionBlurTileMaxY,
    MotionBlurNeighborMax,
    MotionBlurTileVariance,
    GTAOWorkingDepth0,
    GTAOWorkingDepth1,
    GTAOWorkingDepth2,
    GTAOWorkingDepth3,
    GTAOWorkingDepth4,
    GTAOWorkingAOTermNormals,
    GTAOWorkingAOTermVisibility,
    GTAOWorkingEdges,
    GTAOFinalAOTermNormals,
    GTAOFinalAOTermVisibility,
    Count
};

class FieldRenderer
{
  public:
    FieldRenderer(const blackboard::app::Window &window);
    ~FieldRenderer();

    FieldRenderer(const FieldRenderer &) = delete;
    FieldRenderer &operator=(const FieldRenderer &) = delete;
    FieldRenderer(FieldRenderer &&) noexcept = default;
    FieldRenderer &operator=(FieldRenderer &&) noexcept = default;

    void startLoadFieldModel();
    void startLoadRobotModel();
    void startNTClient();
    void render(const blackboard::app::Window &window, const std::shared_ptr<Discord> &discord);

    bool isExiting();

    void setRestartSimulationCallback(std::function<void()> callback);

  private:
    static constexpr uint16_t VIEW_GBUFFER = 0;
    static constexpr uint16_t VIEW_GTAO = 1;
    static constexpr uint16_t VIEW_OIT_MOMENTS = 2;
    static constexpr uint16_t VIEW_OIT = 3;
    static constexpr uint16_t VIEW_POSTPROCESS = 4;
    static constexpr uint16_t VIEW_BLIT = 5;

    void initPBROIT(uint16_t width, uint16_t height);
    void initTonemap();
    void initTAA(uint16_t width, uint16_t height);
    void initGBuffer(uint16_t width, uint16_t height);
    void initMotionBlur(uint16_t width, uint16_t height);
    void initBloom(uint16_t width, uint16_t height);
    void initGTAO(uint16_t width, uint16_t height);

    frc::Pose3d transformPose3dToLocalCoordinates(const frc::Pose3d &pose) const;

    void updateInfo(float cameraNear, float proj[16]);

    void updateOrbitCameraFromInput();

    void loadFieldModel();
    void loadRobotModel();

    void ensureTextures(uint16_t width, uint16_t height);

    void setupMesh(bgfx::Encoder *encoder, const Mesh &mesh, bool isTransparentPrepass = true);

    template <std::ranges::input_range R>
    void drawMeshes(bgfx::Encoder *encoder, R &&meshes, float modelMatrix[2][16])
    {
        for (const auto &mesh : meshes)
        {
            setupMesh(encoder, mesh);
            encoder->setTransform(modelMatrix, 2);
            if (mesh.material.type == MaterialType::Transparent)
            {
                encoder->submit(VIEW_OIT_MOMENTS, programOitMoments);
                setupMesh(encoder, mesh, false);
                encoder->setTransform(modelMatrix, 2);
                encoder->submit(VIEW_OIT, programOit);
            }
            else
            {
                encoder->submit(VIEW_GBUFFER,
                                mesh.material.texture.empty() ? programPBR : programPBRTextured);
            }
        }
    }

    template <std::ranges::forward_range R>
    void drawRobotMeshes(
        bgfx::Encoder *encoder, RobotModel *robotModel, R &&meshes,
        const std::vector<std::pair<const InstanceData &, const RobotData &>> &instances)
    {
        auto instanceableMeshes =
            meshes | std::views::filter([robotModel](const Mesh &mesh)
                                        { return robotModel->isInstanceable(&mesh.material); });

        auto nonInstanceableMeshes =
            meshes | std::views::filter([robotModel](const Mesh &mesh)
                                        { return !robotModel->isInstanceable(&mesh.material); });

        std::vector<InstanceData> instanceData;
        instanceData.reserve(instances.size());
        std::ranges::transform(instances, std::back_inserter(instanceData),
                               [](const auto &pair) { return pair.first; });

        drawMeshesInstanced(encoder, instanceableMeshes, instanceData);

        if (std::ranges::distance(nonInstanceableMeshes) == 0)
        {
            return;
        }

        for (const auto &instance : instances)
        {
            if (robotModel->rslMaterial)
            {
                robotModel->rslMaterial->emissionColor[3] = instance.second.rslEmissionStrength;
            }

            if (robotModel->bumperMaterial)
            {
                robotModel->bumperMaterial->baseColor = instance.second.bumperBaseColor;
            }

            float matrices[2][16];
            instance.first.transform.toMatrix(matrices[0]);
            instance.first.previousTransform.toMatrix(matrices[1]);

            for (auto &mesh : nonInstanceableMeshes)
            {
                setupMesh(encoder, mesh);
                encoder->setTransform(matrices, 2);

                if (mesh.material.type == MaterialType::Transparent)
                {
                    encoder->submit(VIEW_OIT_MOMENTS, programOitMoments);
                    setupMesh(encoder, mesh, false);
                    encoder->setTransform(matrices, 2);
                    encoder->submit(VIEW_OIT, programOit);
                }
                else if (mesh.material.texture == "led")
                {
                    float ledData[4] = {robotModel->ledCount, robotModel->ledAspectRatio, 0.0f,
                                        0.0f};
                    encoder->setUniform(u_ledData, ledData);

                    bgfx::updateTexture2D(
                        instance.second.ledColorTexture.handle, 0, 0, 0, 0,
                        instance.second.ledColorTexture.width,
                        instance.second.ledColorTexture.height,
                        bgfx::copy(
                            instance.second.ledColorData.get(),
                            static_cast<uint32_t>(instance.second.ledColorTexture.width * 4)));

                    encoder->setTexture(1, s_ledColors, instance.second.ledColorTexture.handle,
                                        BGFX_SAMPLER_UVW_CLAMP);

                    encoder->submit(VIEW_GBUFFER, programPBRLed);
                }
                else
                {
                    encoder->submit(VIEW_GBUFFER, programPBR);
                }
            }
        }
    }

    template <std::ranges::input_range R>
    void drawMeshesInstanced(bgfx::Encoder *encoder, R &&meshes,
                             const std::vector<InstanceData> &instanceData)
    {
        // figure out how big of a buffer is available
        uint32_t instanceCount =
            bgfx::getAvailInstanceDataBuffer(instanceData.size(), sizeof(InstanceData));

        bgfx::InstanceDataBuffer idb;
        bgfx::allocInstanceDataBuffer(&idb, instanceCount, sizeof(InstanceData));

        std::memcpy(idb.data, instanceData.data(), instanceCount * sizeof(InstanceData));

        for (const auto &mesh : meshes)
        {
            setupMesh(encoder, mesh);

            encoder->setInstanceDataBuffer(&idb);
            if (mesh.material.type == MaterialType::Transparent)
            {
                encoder->submit(VIEW_OIT_MOMENTS, programOitMomentsInstanced);
                setupMesh(encoder, mesh, false);
                encoder->setInstanceDataBuffer(&idb);
                encoder->submit(VIEW_OIT, programOitInstanced);
            }
            else
            {
                encoder->submit(VIEW_GBUFFER, programPBRInstanced);
            }
        }
    }

    template <std::ranges::input_range R>
    void drawRobot(bgfx::Encoder *encoder, RobotModel *robotModel, R &&instances,
                   const std::function<bool(const DynamicObjectData &)> &componentFilter)
    {
        if (instances.empty())
        {
            return;
        }

        std::vector<std::pair<const InstanceData &, const RobotData &>> robotInstances;
        robotInstances.reserve(std::ranges::distance(instances));
        for (auto &robot : instances)
        {
            robotInstances.emplace_back(robot.dynamicData.instanceData, robot);
        }

        drawRobotMeshes(encoder, robotModel, robotModel->meshes, robotInstances);

        std::map<size_t, std::vector<std::pair<const InstanceData &, const RobotData &>>>
            componentInstancesMap;

        for (const auto &robot : instances)
        {
            for (size_t i = 0; i < robot.components.size(); ++i)
            {
                const auto &component = robot.components[i];
                if (!componentFilter(component))
                {
                    continue;
                }

                componentInstancesMap[i].emplace_back(component.instanceData, robot);
            }
        }

        for (const auto &[index, componentInstances] : componentInstancesMap)
        {
            if (componentInstances.empty())
            {
                continue;
            }

            if (index >= robotModel->components.size())
            {
                blackboard::logger::logger->warn(
                    "Component index {} is out of bounds for robot model with {} components.",
                    index, robotModel->components.size());
                continue;
            }

            const auto &component = robotModel->components[index];
            drawRobotMeshes(encoder, robotModel, component.meshes, componentInstances);
        }
    }

    void drawAprilTags(bgfx::Encoder *encoder);

    void addRobot(std::string_view poseTopic, std::string_view componentPosesTopic,
                  std::string_view rslStateTopic, std::string_view allianceStationTopic,
                  std::string_view ledColorsTopic, std::string_view enabledTopic = "");

    void drawDebugMenu();
    void drawTopUI(ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize);
    void drawViewModeWindow(ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize);
    void drawSettingsWindow(ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize);

    bool exitingFlag = false;
    bool startedLoadingFieldModel = false;
    bool startedLoadingRobotModel = false;

    static constexpr uint8_t LIGHT_COUNT = 6;

    std::array<float, 4> skyColor;
    std::array<std::array<float, 4>, LIGHT_COUNT> lightColor;

    bgfx::UniformHandle u_baseColor;
    bgfx::UniformHandle u_emissionColor;
    bgfx::UniformHandle u_skyColor;
    bgfx::UniformHandle u_gtaoIntensity;
    bgfx::UniformHandle u_info;
    bgfx::UniformHandle u_previousViewProj;
    bgfx::UniformHandle u_previousView;
    bgfx::UniformHandle u_previousProj;
    bgfx::UniformHandle u_jitter;
    bgfx::UniformHandle u_pbrData;
    bgfx::UniformHandle u_ledData;

    bgfx::UniformHandle u_lightPos;
    bgfx::UniformHandle u_lightColor;

    bgfx::UniformHandle u_mbVelocityData;
    bgfx::UniformHandle u_mbBlurData;

    bgfx::UniformHandle u_bloomThreshold;
    bgfx::UniformHandle u_bloomTexelSize;
    bgfx::UniformHandle u_bloomIntensity;

    bgfx::UniformHandle u_lutParams;

    bgfx::UniformHandle u_XeGTAOData;

    bgfx::ProgramHandle programPBR = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programPBRTextured = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programPBRInstanced = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programPBRLed = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programPBRApriltag = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programOit = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programOitInstanced = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programOitMoments = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle programOitMomentsInstanced = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle oitCompProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle tonemapProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle exposureProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle blitProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle presentProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debugNormalsProgram = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle taaResolveProgram = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle mbVelocityProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle mbGuertinTileMaxXProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle mbGuertinTileMaxYProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle mbGuertinNeighborMaxProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle mbGuertinTileVarianceProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle mbGuertinExperimentalBlurProgram = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle bloomDownscaleProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle bloomUpscaleProgram = BGFX_INVALID_HANDLE;

    bgfx::ProgramHandle XeGTAO_PrefilterDepths16x16Program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle XeGTAO_MainPassProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle XeGTAO_DenoiseProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle XeGTAO_debugNormalsProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle XeGTAO_debugVisibilityProgram = BGFX_INVALID_HANDLE;

    ImFont *font;
    blackboard::gui::ImTexture shadowTexture;
    blackboard::gui::ImTexture mainMenuTexture;
    blackboard::gui::ImTexture settingsTexture;
    blackboard::gui::ImTexture viewModeTexture;
    blackboard::gui::ImTexture restartJavaTexture;

    Texture gMomentsTex;
    Texture gTotalDepthTex;
    Texture gAccumTex;

    Texture gbufAlbedo;
    Texture gbufEmission;
    Texture gbufNormal;
    Texture gbufPBRData;
    Texture gbufVelocity;
    Texture gFullVelocity;
    Texture gbufDepth;

    Texture gOutputColor;

    Texture gTAABuffer0;
    Texture gTAABuffer1;

    Texture gMBTileMaxX;
    Texture gMBTileMax;
    Texture gMBNeighborMax;
    Texture gMBTileVariance;
    Texture gMBVelocity;
    Texture gMBOutputColor;

    Texture gGTAOWorkingDepth;
    Texture gGTAOWorkingAOTerm;
    Texture gGTAOWorkingEdges;
    Texture gGTAOHilbertLut;
    Texture gGTAOFinalAOTerm;

    Texture bloomDirtMask;

    Texture carpetBaseColor;
    Texture carpetBump;
    Texture apriltagTexture;
    Texture ledMaskTexture;

    Texture tonemappingLut;

    FrameBuffer gBufFbo;
    FrameBuffer gOitFbo;
    FrameBuffer gOitMomentsFbo;

    bgfx::UniformHandle s_texPresent;

    bgfx::UniformHandle s_taaHistory;

    bgfx::UniformHandle s_color;
    bgfx::UniformHandle s_velocity;
    bgfx::UniformHandle s_depth;

    bgfx::UniformHandle s_momentsTex;
    bgfx::UniformHandle s_totalDepthTex;

    bgfx::UniformHandle s_mbTileMaxX;
    bgfx::UniformHandle s_mbTileMax;
    bgfx::UniformHandle s_mbNeighborMax;
    bgfx::UniformHandle s_mbTileVariance;

    bgfx::UniformHandle s_bloomInput;
    bgfx::UniformHandle s_bloomDirt;

    bgfx::UniformHandle s_lut;

    bgfx::UniformHandle s_baseColor;
    bgfx::UniformHandle s_bump;
    bgfx::UniformHandle s_apriltags;
    bgfx::UniformHandle s_ledMask;
    bgfx::UniformHandle s_ledColors;

    bgfx::UniformHandle s_workingAOTerm;
    bgfx::UniformHandle s_workingEdges;

    float bloomThreshold = 2.0f;
    float bloomKnee = 0.1f;
    float bloomIntensity = 1.0f;
    float bloomDirtIntensity = 1.1f;

    float tonemappingExposure = -2.0f;

    float gtaoEffectRadius = 0.5f;
    float gtaoRadiusMultiplier = 1.457f;
    float gtaoEffectFalloffRange = 0.615f;
    float gtaoSampleDistributionPower = 2.0f;
    float gtaoThinOccluderCompensation = 0.0f;
    float gtaoDepthMipSamplingOffset = 3.30f;
    float gtaoFinalValuePower = 2.2f;
    float gtaoDenoiseBlurBeta = 1.2f;

    float gtaoIntensity = 1.0f;
    float gtaoDirectIntensity = 1.0f;
    float gtaoBentNormalIntensity = 0.7f; // 0.7 looks a bit better than 1.0 with metallic materials

    float fieldModelMatrix[2][16];

    WPILibCoordinateSystem coordinateSystem = WPILibCoordinateSystem::CenterRed;
    float fieldWidthMeters = 1.0f;
    float fieldHeightMeters = 1.0f;

    static constexpr auto DISCORD_UPDATE_INTERVAL = std::chrono::seconds(5);

    std::chrono::steady_clock::time_point lastDiscordUpdateTime =
        std::chrono::steady_clock::now() - DISCORD_UPDATE_INTERVAL;

    std::vector<AprilTagInstanceData> aprilTags;

    Mesh aprilTagMesh{};

    std::vector<bx::Vec3> driverStationCameraPositions;
    bx::Vec3 driverStationCameraTarget{0.0f, 0.0f, 0.5f};
    bx::Vec3 lastDriverStationCameraTarget{0.0f, 0.0f, 0.5f};

    std::future<void> fieldModelLoadingFuture;
    std::future<void> robotModelLoadingFuture;

    nt::NetworkTableInstance ntInst;

#if GAME_YEAR == 2026
    std::unique_ptr<Rebuilt2026FMSUI> fmsUI;
#endif

    DebugView debugView = DebugView::None;

    bool freezeTemporalEffects = false;

    std::vector<Mesh> fieldMeshes;

    std::vector<RobotModel> robotModels;
    std::unordered_map<RobotModel *, std::vector<RobotData>> robots;
    std::vector<GamePieceData> gamePieces;

    std::function<void()> restartSimulationCallback;

    OrbitCamera orbitCamera;

    float previousViewProj[16] = {0};
    float previousView[16] = {0};
    float previousProj[16] = {0};

    float curTime = 0.0f;
    float previousJitterX = 0.0f;
    float previousJitterY = 0.0f;
    bool firstFrame = true;

    bool firstTAAFrame = true;
    bool taaUseBuffer1 = false;

    uint8_t jitterIndex = 0;
    uint8_t mbIndex = 0;
    uint8_t gtaoNoiseIndex = 0;

    bool createdFieldMeshBuffers = false;
    bool createdRobotMeshBuffers = false;
    int currentDataUpdateIndex = 0;

    bool isTopUISummoned = false;
    std::chrono::steady_clock::time_point firstTopUISummonTime;
    std::chrono::steady_clock::time_point lastTopUISummonTime;

    bool showSettings = false;
    bool showViewMode = false;
};