// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>
#include "audio_core/common.h"
#include "audio_core/voice_context.h"
#include "common/common_types.h"

namespace Core::Memory {
class Memory;
}

namespace AudioCore {
class MixContext;
class SplitterContext;
class ServerSplitterDestinationData;
class ServerMixInfo;
class EffectContext;
class EffectBase;
struct AuxInfoDSP;
struct I3dl2ReverbParams;
struct I3dl2ReverbState;
using MixVolumeBuffer = std::array<float, AudioCommon::MAX_MIX_BUFFERS>;

class CommandGenerator {
public:
    explicit CommandGenerator(AudioCommon::AudioRendererParameter& worker_params_,
                              VoiceContext& voice_context_, MixContext& mix_context_,
                              SplitterContext& splitter_context_, EffectContext& effect_context_,
                              Core::Memory::Memory& memory_);
    ~CommandGenerator();

    void ClearMixBuffers();
    void GenerateVoiceCommands();
    void GenerateVoiceCommand(ServerVoiceInfo& voice_info);
    void GenerateSubMixCommands();
    void GenerateFinalMixCommands();
    void PreCommand();
    void PostCommand();

    [[nodiscard]] std::span<s32> GetChannelMixBuffer(s32 channel);
    [[nodiscard]] std::span<const s32> GetChannelMixBuffer(s32 channel) const;
    [[nodiscard]] std::span<s32> GetMixBuffer(std::size_t index);
    [[nodiscard]] std::span<const s32> GetMixBuffer(std::size_t index) const;
    [[nodiscard]] std::size_t GetMixChannelBufferOffset(s32 channel) const;

    [[nodiscard]] std::size_t GetTotalMixBufferCount() const;

private:
    void GenerateDataSourceCommand(ServerVoiceInfo& voice_info, VoiceState& dsp_state, s32 channel);
    void GenerateBiquadFilterCommandForVoice(ServerVoiceInfo& voice_info, VoiceState& dsp_state,
                                             s32 mix_buffer_count, s32 channel);
    void GenerateVolumeRampCommand(float last_volume, float current_volume, s32 channel,
                                   s32 node_id);
    void GenerateVoiceMixCommand(const MixVolumeBuffer& mix_volumes,
                                 const MixVolumeBuffer& last_mix_volumes, VoiceState& dsp_state,
                                 s32 mix_buffer_offset, s32 mix_buffer_count, s32 voice_index,
                                 s32 node_id);
    void GenerateSubMixCommand(ServerMixInfo& mix_info);
    void GenerateMixCommands(ServerMixInfo& mix_info);
    void GenerateMixCommand(std::size_t output_offset, std::size_t input_offset, float volume,
                            s32 node_id);
    void GenerateFinalMixCommand();
    void GenerateBiquadFilterCommand(s32 mix_buffer, const BiquadFilterParameter& params,
                                     std::array<s64, 2>& state, std::size_t input_offset,
                                     std::size_t output_offset, s32 sample_count, s32 node_id);
    void GenerateDepopPrepareCommand(VoiceState& dsp_state, std::size_t mix_buffer_count,
                                     std::size_t mix_buffer_offset);
    void GenerateDepopForMixBuffersCommand(std::size_t mix_buffer_count,
                                           std::size_t mix_buffer_offset, s32 sample_rate);
    void GenerateEffectCommand(ServerMixInfo& mix_info);
    void GenerateI3dl2ReverbEffectCommand(s32 mix_buffer_offset, EffectBase* info, bool enabled);
    void GenerateBiquadFilterEffectCommand(s32 mix_buffer_offset, EffectBase* info, bool enabled);
    void GenerateAuxCommand(s32 mix_buffer_offset, EffectBase* info, bool enabled);
    [[nodiscard]] ServerSplitterDestinationData* GetDestinationData(s32 splitter_id, s32 index);

    s32 WriteAuxBuffer(AuxInfoDSP& dsp_info, VAddr send_buffer, u32 max_samples,
                       std::span<const s32> data, u32 sample_count, u32 write_offset,
                       u32 write_count);
    s32 ReadAuxBuffer(AuxInfoDSP& recv_info, VAddr recv_buffer, u32 max_samples,
                      std::span<s32> out_data, u32 sample_count, u32 read_offset, u32 read_count);

    void InitializeI3dl2Reverb(I3dl2ReverbParams& info, I3dl2ReverbState& state,
                               std::vector<u8>& work_buffer);
    void UpdateI3dl2Reverb(I3dl2ReverbParams& info, I3dl2ReverbState& state, bool should_clear);
    // DSP Code
    template <typename T>
    s32 DecodePcm(ServerVoiceInfo& voice_info, VoiceState& dsp_state, s32 sample_start_offset,
                  s32 sample_end_offset, s32 sample_count, s32 channel, std::size_t mix_offset);
    s32 DecodeAdpcm(ServerVoiceInfo& voice_info, VoiceState& dsp_state, s32 sample_start_offset,
                    s32 sample_end_offset, s32 sample_count, s32 channel, std::size_t mix_offset);
    void DecodeFromWaveBuffers(ServerVoiceInfo& voice_info, std::span<s32> output,
                               VoiceState& dsp_state, s32 channel, s32 target_sample_rate,
                               s32 sample_count, s32 node_id);

    AudioCommon::AudioRendererParameter& worker_params;
    VoiceContext& voice_context;
    MixContext& mix_context;
    SplitterContext& splitter_context;
    EffectContext& effect_context;
    Core::Memory::Memory& memory;
    std::vector<s32> mix_buffer{};
    std::vector<s32> sample_buffer{};
    std::vector<s32> depop_buffer{};
    bool dumping_frame{false};
};
} // namespace AudioCore
