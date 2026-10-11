#include "json_io.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string output_file;
    std::string module_override;
    bool to_stdout = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) input_file = argv[++i];
        else if ((arg == "-o" || arg == "--output") && i + 1 < argc) output_file = argv[++i];
        else if ((arg == "-m" || arg == "--module") && i + 1 < argc) module_override = argv[++i];
        else if (arg == "--stdout") to_stdout = true;
        else if (arg == "-h" || arg == "--help") {
            std::cout
                << "SO-Sim — núcleo C++ (módulos 1-3 + SO integrado)\n"
                << "Uso: sosim -i input.json -o output.json [opciones]\n"
                << "  -i, --input <file>    JSON de escenario\n"
                << "  -o, --output <file>   JSON de resultados\n"
                << "  -m, --module <name>   schedule | sync | memory | os | all\n"
                << "  --stdout              imprime JSON en stdout\n";
            return 0;
        }
    }

    if (input_file.empty()) {
        std::cerr << "ERROR: falta -i input.json\n";
        return 1;
    }

    nlohmann::json root;
    std::string err;
    if (!load_json_file(input_file, root, err)) {
        std::cerr << "ERROR: " << err << "\n";
        return 1;
    }

    nlohmann::json result;
    try {
        result = run_from_input(root, module_override);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: escenario inválido (" << e.what() << ")\n";
        return 1;
    }
    if (!result.value("ok", true)) {
        std::cerr << "ERROR: " << result.value("error", "desconocido") << "\n";
        return 1;
    }

    if (to_stdout || output_file.empty()) {
        std::cout << result.dump(2) << "\n";
    }
    if (!output_file.empty()) {
        if (!write_json_file(output_file, result, err)) {
            std::cerr << "ERROR: " << err << "\n";
            return 1;
        }
        std::cerr << "OK escrito en " << output_file << "\n";
    }
    return 0;
}
