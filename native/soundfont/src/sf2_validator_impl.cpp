#include "olysf2sampler/soundfont/validator.hpp"

#include <sstream>

namespace olysf2sampler::soundfont {

namespace {

void checkZoneReferences(const std::vector<Zone>& zones, std::uint16_t targetGeneratorType,
                         std::size_t targetCount, const char* targetKindName,
                         const std::string& ownerName, ValidationReport& report) {
    for (const Zone& zone : zones) {
        for (const Generator& g : zone.generators) {
            if (g.type != targetGeneratorType) {
                continue;
            }
            // El valor del generador es int16_t pero los índices SF2 son
            // no-negativos por diseño; un valor negativo aquí ya es en
            // sí mismo una señal de dato corrupto/no confiable.
            if (g.value < 0 || static_cast<std::size_t>(g.value) >= targetCount) {
                std::ostringstream msg;
                msg << "'" << ownerName << "' referencia " << targetKindName << " #" << g.value
                    << ", fuera de rango (hay " << targetCount << ").";
                report.issues.push_back(ValidationIssue{msg.str(), /*isFatal=*/true});
                report.isValid = false;
            }
        }
    }
}

void checkSampleHeaderRanges(const SoundFontModel& model, ValidationReport& report) {
    for (const SampleHeader& s : model.samples) {
        const std::size_t poolSize = model.sampleData.size();
        bool rangeOk = s.startOffset <= s.endOffset &&
                       static_cast<std::size_t>(s.endOffset) <= poolSize;
        if (!rangeOk) {
            std::ostringstream msg;
            msg << "Sample '" << s.name << "': startOffset/endOffset (" << s.startOffset << "/"
                << s.endOffset << ") fuera del rango de sampleData (" << poolSize << " frames).";
            report.issues.push_back(ValidationIssue{msg.str(), /*isFatal=*/true});
            report.isValid = false;
            continue;
        }
        bool loopOk = s.loopStart <= s.loopEnd && s.loopStart >= s.startOffset &&
                      s.loopEnd <= s.endOffset;
        if (!loopOk) {
            std::ostringstream msg;
            msg << "Sample '" << s.name << "': loop [" << s.loopStart << "," << s.loopEnd
                << "] fuera de [" << s.startOffset << "," << s.endOffset << "].";
            // No fatal: un loop inconsistente es reproducible igual sin
            // loop (degradación, no corrupción de memoria) — se marca
            // como advertencia, no como error bloqueante.
            report.issues.push_back(ValidationIssue{msg.str(), /*isFatal=*/false});
        }
    }
}

}  // namespace

class DefaultSf2Validator final : public Sf2Validator {
public:
    // NO noexcept (fix §9, mismo razonamiento que parser/writer):
    // ostringstream y push_back alojan memoria; ruta offline, no
    // realtime. Antes prometía noexcept sin poder garantizarlo.
    ValidationReport validate(const SoundFontModel& model) const override {
        try {
            return validateImpl(model);
        } catch (const std::exception& e) {
            ValidationReport report;
            report.isValid = false;
            report.issues.push_back(ValidationIssue{
                std::string("Excepción inesperada durante la validación: ") + e.what(),
                /*isFatal=*/true});
            return report;
        }
    }

private:
    ValidationReport validateImpl(const SoundFontModel& model) const {
        ValidationReport report;
        report.isValid = true;

        checkSampleHeaderRanges(model, report);

        for (const Instrument& instrument : model.instruments) {
            checkZoneReferences(instrument.zones, GeneratorOperator::kGeneratorSampleID,
                                model.samples.size(), "sample", instrument.name, report);
        }
        for (const Preset& preset : model.presets) {
            checkZoneReferences(preset.zones, GeneratorOperator::kGeneratorInstrument,
                                model.instruments.size(), "instrument", preset.name, report);
        }

        if (model.metadata.bankName.empty()) {
            report.issues.push_back(
                ValidationIssue{"metadata.bankName está vacío (INAM es obligatorio en la spec).",
                                /*isFatal=*/false});
        }

        return report;
    }
};

std::unique_ptr<Sf2Validator> createDefaultSf2Validator() {
    return std::make_unique<DefaultSf2Validator>();
}

}  // namespace olysf2sampler::soundfont
