"""Execute a linked test ELF with Unicorn. pip install unicorn pyelftools."""
import sys
from pathlib import Path
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0

path = Path(sys.argv[1])
uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x20000)
uc.mem_map(0x20000000, 0x10000)
with path.open('rb') as stream:
    elf = ELFFile(stream)
    for section in elf.iter_sections():
        if section['sh_flags'] & 2 and section['sh_size']:
            data = bytes(section['sh_size']) if section['sh_type'] == 'SHT_NOBITS' else section.data()
            uc.mem_write(section['sh_addr'], data)
    entry = elf.get_section_by_name('.symtab').get_symbol_by_name('TestMain')[0]['st_value']

stop = 0x0801FFF0
finished = False
def on_code(engine, address, size, user_data):
    global finished
    if address == stop:
        finished = True
        engine.emu_stop()

uc.hook_add(UC_HOOK_CODE, on_code)
uc.reg_write(UC_ARM_REG_SP, 0x2000F000)
uc.reg_write(UC_ARM_REG_LR, stop | 1)
uc.emu_start(entry | 1, 0, count=2000000)
assert finished, 'Test did not return (possible blocking loop).'
result = uc.reg_read(UC_ARM_REG_R0)
assert result == 0, f'Test failed: code {result}'
print(f'PASS: {path.name}; Cortex-M3 C tests returned 0')
