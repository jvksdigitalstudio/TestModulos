#include "olysf2sampler/sampler/mapping.hpp"

namespace olysf2sampler::sampler {

namespace {

class LinearScanKeyVelocityMapping final : public KeyVelocityMapping {
public:
    void setZones(std::vector<MappingZone> zones) override { zones_ = std::move(zones); }

    std::vector<std::uint64_t> resolve(int midiNote, int velocity) const noexcept override {
        std::vector<std::uint64_t> result;
        for (const MappingZone& zone : zones_) {
            bool noteInRange = midiNote >= zone.lowNote && midiNote <= zone.highNote;
            bool velocityInRange = velocity >= zone.lowVelocity && velocity <= zone.highVelocity;
            if (noteInRange && velocityInRange) {
                result.push_back(zone.sampleId);
            }
        }
        return result;
    }

private:
    std::vector<MappingZone> zones_;
};

}  // namespace

std::unique_ptr<KeyVelocityMapping> createKeyVelocityMapping() {
    return std::make_unique<LinearScanKeyVelocityMapping>();
}

}  // namespace olysf2sampler::sampler
