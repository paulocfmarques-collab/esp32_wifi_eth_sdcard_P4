#include "CrashDiagnostics.h"
#include <esp_core_dump.h>

namespace
{
void endereco(String& out, uint32_t value)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "0x%08lx", (unsigned long)value);
    out += buffer;
}
} // namespace

String CrashDiagnostics::lerArquivarEApagar(StorageManager& storage, const char* motivoReset)
{
    String resultado = "Ultimo reset: " + String(motivoReset) + "\n";
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    esp_err_t erro = esp_core_dump_image_check();
    if (erro != ESP_OK)
        return resultado + "Sem core dump valido: " + esp_err_to_name(erro) + "\n";

    char motivo[200] = {};
    if (esp_core_dump_get_panic_reason(motivo, sizeof(motivo)) == ESP_OK)
    {
        motivo[sizeof(motivo) - 1] = 0;
        resultado += "Panic salvo: " + String(motivo) + "\n";
    }
    esp_core_dump_summary_t resumo = {};
    erro = esp_core_dump_get_summary(&resumo);
    if (erro != ESP_OK)
        return resultado + "Falha ao ler resumo: " + esp_err_to_name(erro) +
               "\nRegistro interno preservado.\n";
    if (!resultado.reserve(sizeof(resumo) * 5 + 2048))
        return resultado + "Memoria insuficiente para o relatorio.\nRegistro interno preservado.\n";
    // Bound SDK strings even if the crashed task had damaged memory.
    resumo.exc_task[sizeof(resumo.exc_task) - 1] = 0;
    resumo.app_elf_sha256[sizeof(resumo.app_elf_sha256) - 1] = 0;
    resultado += "Tarefa: " + String(resumo.exc_task) + "\nPC: ";
    endereco(resultado, resumo.exc_pc);
#if CONFIG_IDF_TARGET_ARCH_RISCV
    resultado += "\nRA: ";
    endereco(resultado, resumo.ex_info.ra);
    resultado += "\nSP: ";
    endereco(resultado, resumo.ex_info.sp);
    resultado += "\nMCAUSE: ";
    endereco(resultado, resumo.ex_info.mcause);
    resultado += "\nMTVAL: ";
    endereco(resultado, resumo.ex_info.mtval);
    resultado += "\nBacktrace: requer o arquivo ELF do firmware que sofreu o crash "
                 "(RISC-V).\nStack dump bruto:";
    const size_t tamanho =
        min(size_t(resumo.exc_bt_info.dump_size), sizeof(resumo.exc_bt_info.stackdump));
    for (size_t i = 0; i < tamanho; i += 4)
    {
        if (i % 16 == 0)
        {
            resultado += '\n';
            endereco(resultado, resumo.ex_info.sp + i);
            resultado += ":";
        }
        // A final partial word is printed byte by byte rather than reading past the dump.
        if (i + 4 <= tamanho)
        {
            uint32_t word;
            memcpy(&word, resumo.exc_bt_info.stackdump + i, sizeof(word));
            resultado += ' ';
            endereco(resultado, word);
        }
        else
        {
            for (size_t j = i; j < tamanho; ++j)
            {
                char byte[5];
                snprintf(byte, sizeof(byte), " %02x", resumo.exc_bt_info.stackdump[j]);
                resultado += byte;
            }
        }
    }
#else
    resultado += "\nBacktrace:";
    for (uint32_t i = 0; i < resumo.exc_bt_info.depth &&
                         i < sizeof(resumo.exc_bt_info.bt) / sizeof(resumo.exc_bt_info.bt[0]);
         ++i)
    {
        resultado += ' ';
        endereco(resultado, resumo.exc_bt_info.bt[i]);
    }
    resultado += "\nBacktrace corrompido: " + String(resumo.exc_bt_info.corrupted ? "SIM" : "NAO");
#endif
    resultado +=
        "\nELF SHA256: " + String(reinterpret_cast<const char*>(resumo.app_elf_sha256)) + "\n";
    String caminho, erroSd;
    if (!storage.salvarCrash(resultado, caminho, erroSd))
        return resultado + "Falha ao arquivar crash: " + erroSd +
               "\nRegistro interno preservado.\n";
    resultado += "Crash salvo no SD: " + caminho + "\n";
    erro = esp_core_dump_image_erase();
    resultado += erro == ESP_OK
                     ? "Registro de crash apagado apos leitura.\n"
                     : "Falha ao apagar registro de crash: " + String(esp_err_to_name(erro)) + "\n";
#else
    resultado += "Core dump nao habilitado neste firmware.\n";
#endif
    return resultado;
}
