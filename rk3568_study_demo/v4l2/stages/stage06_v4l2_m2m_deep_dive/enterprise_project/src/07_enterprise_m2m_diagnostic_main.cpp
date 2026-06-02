#include "01_cli_config.hpp"
#include "06_m2m_diagnostic_service.hpp"

using namespace stage06_enterprise;

int main(int argc, char** argv) {
    CliConfig config;
    std::string error;
    if (!parse_cli(argc, argv, &config, &error)) {
        std::cerr << "argument error: " << error << "\n";
        print_usage(argv[0]);
        return 2;
    }

    /*
     * 主入口只做三件事：解析参数、创建服务、运行服务。
     * 这样读代码时可以直接跳到 06_m2m_diagnostic_service.cpp 看状态机主流程。
     */
    M2mDiagnosticService service(config);
    return service.run();
}
