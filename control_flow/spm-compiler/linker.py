#!/usr/bin/env python3

import re
import string
import os

from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError

import sancus.config
import sancus.paths

from common import *

MAC_SIZE = int(sancus.config.SECURITY / 8)

def rename_syms_sects(file, sym_map, sect_map):
    args = []
    for old, new in sym_map.items():
        args += ['--redefine-sym', '{}={}'.format(old, new)]
    for old, new in sect_map.items():
        args += ['--rename-section', '{}={}'.format(old, new)]

    out_file = get_tmp('.o')
    args += [file, out_file]
    call_prog('msp430-objcopy', args)
    return out_file


def parse_size(val):
    try:
        return int(val)
    except ValueError:
        match = re.match(r'(\d+)K', val)
        if not match:
            raise ValueError('Not a valid size expression: ' + val)
        return int(match.group(1)) * 1024


def get_symbol(elf_file, name):
    from elftools.elf.elffile import SymbolTableSection
    for section in elf_file.iter_sections():
        if isinstance(section, SymbolTableSection):
            for symbol in section.iter_symbols():
                sym_section = symbol['st_shndx']
                if symbol.name == name and sym_section != 'SHN_UNDEF':
                    return symbol

## jo: a custom argparse type for a readable directory path (from
## http://stackoverflow.com/questions/11415570/directory-path-types-with-argparse)
def readable_dir(prospective_dir):
  if not os.path.isdir(prospective_dir):
    raise argparse.ArgumentTypeError(
        "readable_dir:{0} is not a valid path".format(prospective_dir))
  if os.access(prospective_dir, os.R_OK):
    return prospective_dir
  else:
    raise argparse.ArgumentTypeError(
        "readable_dir:{0} is not a readable dir".format(prospective_dir))

parser = argparse.ArgumentParser(description='Sancus module linker.',
                                 parents=[get_common_parser()])
parser.add_argument('--standalone', action='store_true')
parser.add_argument('--ram-size',
                    choices=['128', '256', '512', '1K', '2K', '4K', '5K',
                             '8K', '10K', '16K', '24K', '32K'],
                    default='10K')
parser.add_argument('--rom-size',
                    choices=['1K', '2K', '4K', '8K', '12K', '16K', '24K',
                             '32K', '41K', '48K', '51K', '54K', '55K'],
                    default='48K')
parser.add_argument('-rdynamic', action='store_true')
parser.add_argument('--sm-stack-size',
                    help='Stack size for the module (in bytes)',
                    type=positive_int,
                    default=256,
                    metavar='size')
parser.add_argument('--print-default-libs',
                    help='Print libraries that are always linked',
                    action='store_true')

## jo: added arguments to allow passing a custom sm_entry / sm_exit for scheduler SM
parser.add_argument("--sched-asm-path",
                    help="Custom path to the assembly sm_entry and sm_exit stubs " \
                    "for the SM named 'scheduler' (if any)",
                    type=readable_dir, metavar="PATH",
                    default=sancus.paths.get_data_path())

args, cli_ld_args = parser.parse_known_args()
set_args(args)

if args.print_default_libs:
    lib_dir = sancus.paths.get_data_path() + '/lib'
    print(lib_dir + '/libsancus-sm-support.a')
    if args.standalone:
        print(lib_dir + '/libsancus-host-support.a')
    sys.exit(0)

# find all defined SMs
sms = set()
sms_table_order = {}
sms_entries = {}
sms_calls = {}
existing_sms = set()
existing_macs = []

for file_name in args.in_files:
    try:
        with open(file_name, 'rb') as file:
            elf_file = ELFFile(file)
            for section in elf_file.iter_sections():
                name = section.name.decode('ascii')
                match = re.match(r'.sm.(\w+).text', name)
                if match:
                    sm_name = match.group(1)
                    # if the following symbol exists, we assume the SM is
                    # created manually and we will output it "as is"
                    label = '__sm_{}_public_start'.format(sm_name)
                    if get_symbol(elf_file, label.encode('ascii')) is None:
                        sms.add(sm_name)
                    else:
                        existing_sms.add(sm_name)
                    continue

                match = re.match(r'.rela.sm.(\w+).table', name)
                if match:
                    sm_name = match.group(1)
                    if not sm_name in sms_table_order:
                        sms_table_order[sm_name] = []
                        sms_entries[sm_name] = []

                    sms_table_order[sm_name].append(file_name)

                    #find entry points of the SM in this file
                    symtab = elf_file.get_section(section['sh_link'])
                    entries = [(rel['r_offset'],
                                symtab.get_symbol(rel['r_info_sym']).name)
                                    for rel in section.iter_relocations()]
                    entries.sort()
                    sms_entries[sm_name] += \
                        [entry.decode('ascii') for _, entry in entries]
                    continue

                match = re.match(r'.rela.sm.(\w+).text', name)
                if match:
                    sm_name = match.group(1)

                    #find call from this SM to others
                    symtab = elf_file.get_section(section['sh_link'])
                    for rel in section.iter_relocations():
                        sym_name = symtab.get_symbol(rel['r_info_sym']).name
                        rel_match = re.match(rb'__sm_(\w+)_entry', sym_name)
                        if rel_match and rel_match.group(1) != sm_name:
                            if not sm_name in sms_calls:
                                sms_calls[sm_name] = set()
                            callee_name = rel_match.group(1).decode('ascii')
                            sms_calls[sm_name].add(callee_name)
                    continue

                match = re.match(r'.sm.(\w+).mac.(\w+)', name)
                if match:
                    existing_macs.append((match.group(1), match.group(2)))
                    continue
    except IOError as e:
        fatal_error(str(e))
    except ELFError as e:
        debug('Not checking {} for SMs because it is not a valid '
              'ELF file ({})'.format(file_name, e))

if len(sms) > 0:
    info('Found new Sancus modules:')
    for sm in sms:
        info(' * {}:'.format(sm))
        if sm in sms_entries:
            #entry_names = [entry.decode('ascii') for entry in sms_entries[sm]]
            info('  - Entries: {}'.format(', '.join(sms_entries[sm])))
        else:
            info('  - No entries')
        if sm in sms_calls:
            info('  - Calls:   {}'.format(', '.join(sms_calls[sm])))
        else:
            info('  - No calls to other modules')
else:
    info('No new Sancus modules found')

if len(existing_sms) > 0:
    info('Found existing Sancus modules:')
    for sm in existing_sms:
        info(' * {}'.format(sm))
else:
    info('No existing Sancus modules found')

# create output sections for the the SM to be inserted in the linker script
text_section = '''.text.sm.{0} :
  {{
    . = ALIGN(2);
    __sm_{0}_public_start = .;
    {1}
    {2}
    *(.sm.{0}.text)
    . = ALIGN(2);
    __sm_{0}_table = .;
    {3}
    . = ALIGN(2);
    __sm_{0}_public_end = .;
  }}'''

## jo: added the following private vars per sm:
##  __sm_sad        scheduler address
##  __sm_sid        scheduler ID
##  __sm_vep        scheduler report_violation entry point
##  __sm_yep        scheduler yield entry point
##  __sm_gep        scheduler get_cur_thr_id entry point
##  __sm_cur_thr_id current thread identfier field
##
data_section = '''. = ALIGN(2);
    __sm_{0}_secret_start = .;
    *(.sm.{0}.data)
    {1}
    . += {2};
    __sm_{0}_stack_init = .;
    __sm_{0}_sp = .;
    . += 2;
    __sm_{0}_sad = .;
    . += 2;
    __sm_{0}_sid = .;
    . += 2;
    __sm_{0}_vep = .;
    . += 2;    
    __sm_{0}_yep = .;
    . += 2;
    __sm_{0}_gep = .;
    . += 2;
    __sm_{0}_thr = .;
    . += 2;        
    . = ALIGN(2);
    __sm_{0}_secret_end = .;'''

mac_section = '''.data.sm.{0}.mac.{1} :
  {{
    . = ALIGN(2);
    __sm_{0}_mac_{1} = .;
    BYTE(0x00); /* without this, this section will be empty in the binary */
    . += {2} - 1;
  }}'''

existing_text_section = '''.text.sm.{0} :
  {{
    *(.sm.{0}.text)
  }}'''

existing_data_section = '''. = ALIGN(2);
  *(.sm.{0}.data)'''

existing_mac_section = '''.data.sm.{0}.mac.{1} :
  {{
    *(.sm.{0}.mac.{1})
  }}'''

if args.standalone:
    text_section += ' > REGION_TEXT'
    mac_section += ' > REGION_TEXT'
    existing_text_section += ' > REGION_TEXT'
    existing_mac_section += ' > REGION_TEXT'

text_sections = []
data_sections = []
mac_sections = []
symbols = []
for sm in sms:
    nentries = '__sm_{}_nentries'.format(sm)
    sym_map = {'__sm_entry'         : '__sm_{}_entry'.format(sm),
               '__sm_nentries'      : nentries,
               '__sm_table'         : '__sm_{}_table'.format(sm),
               '__sm_sp'            : '__sm_{}_sp'.format(sm),
               ## jo: scheduler SM address
               '__sm_sad'           : '__sm_{}_sad'.format(sm),
               ## jo: scheduler SM ID
               '__sm_sid'           : '__sm_{}_sid'.format(sm),
               ## jo: scheduler SM report_violation entry point
               '__sm_vep'           : '__sm_{}_vep'.format(sm),
               ## jo: scheduler SM yield entry point
               '__sm_yep'           : '__sm_{}_yep'.format(sm),
               ## jo: scheduler SM get_current_thr_id entry point
               '__sm_gep'           : '__sm_{}_gep'.format(sm),
               ## jo: current thread id field
               '__sm_thr'           : '__sm_{}_thr'.format(sm),               
               ## jo: assembly function to report entry failure to scheduler SM
               '__entry_violation'  : '__sm_{}_entry_violation'.format(sm),
               ## jo: assembly function to get the current thr id from the scheduler
               '__get_cur_thr_id'   : '__sm_{}_get_cur_thr_id'.format(sm),
               '__ret_entry'        : '__sm_{}_ret_entry'.format(sm),
               '__sm_exit'          : '__sm_{}_exit'.format(sm),
               '__sm_stack_init'    : '__sm_{}_stack_init'.format(sm),
               '__sm_verify'        : '__sm_{}_verify'.format(sm)}
    sect_map = {'.sm.text' : '.sm.{}.text'.format(sm)}
    
    tables = []
    if sm in sms_table_order:
        tables = ['{}(.sm.{}.table)'.format(file, sm)
                      for file in sms_table_order[sm]]

    id_syms = []
    if sm in sms_calls:
        for callee in sms_calls[sm]:
            mac_sections.append(mac_section.format(sm, callee, MAC_SIZE))
            id_syms += ['__sm_{}_id_{} = .;'.format(sm, callee), '. += 2;']

        object = sancus.paths.get_data_path() + '/sm_verify.o'
        verify_file = rename_syms_sects(object, sym_map, sect_map)
        args.in_files.append(verify_file)

    ## jo: use a user-provided path for the "scheduler" SM asm stubs
    if sm == "scheduler":
        info("Using sched-asm-path: %s", args.sched_asm_path)
        entry_file = rename_syms_sects(args.sched_asm_path + '/sm_entry.o',
                                   sym_map, sect_map)
        exit_file = rename_syms_sects(args.sched_asm_path + '/sm_exit.o',
                                  sym_map, sect_map)
    else:
        entry_file = rename_syms_sects(sancus.paths.get_data_path() + '/sm_entry.o',
                                   sym_map, sect_map)
        exit_file = rename_syms_sects(sancus.paths.get_data_path() + '/sm_exit.o',
                                  sym_map, sect_map)
    
    args.in_files += [entry_file, exit_file]

    text_sections.append(text_section.format(sm, entry_file, exit_file,
                                             '\n    '.join(tables)))
    data_sections.append(data_section.format(sm, '\n    '.join(id_syms),
                                             args.sm_stack_size))

    symbols.append('{} = {};'.format(nentries, len(sms_entries[sm])))
    for idx, entry in enumerate(sms_entries[sm]):
        sym_name = '__sm_{}_entry_{}_idx'.format(sm, entry)
        symbols.append('{} = {};'.format(sym_name, idx))

for sm in existing_sms:
    text_sections.append(existing_text_section.format(sm))
    data_sections.append(existing_data_section.format(sm))

for caller, callee in existing_macs:
    mac_sections.append(existing_mac_section.format(caller, callee))

text_sections = '\n  '.join(text_sections)
data_sections = '\n    '.join(data_sections)
mac_sections = '\n  '.join(mac_sections)
symbols = '\n'.join(symbols)

tmp_ldscripts_path = get_tmp_dir()
template_path = sancus.paths.get_data_path()
msp_paths = get_msp_paths()

if args.standalone:
    if args.mcu:
        ldscripts_path = msp_paths['ldscripts']
        mcu_ldscripts_path = ldscripts_path + '/' + args.mcu

        if os.path.exists(mcu_ldscripts_path):
            info('Using linker scripts path: ' + mcu_ldscripts_path)
        else:
            fatal_error('No linker scripts found for MCU ' + args.mcu)
    else:
        mcu_ldscripts_path = tmp_ldscripts_path
        shutil.copy(template_path + '/periph.x', mcu_ldscripts_path)

        ram_length = parse_size(args.ram_size)
        rom_length = parse_size(args.rom_size)
        rom_origin = 0x10000 - rom_length
        with open(template_path + '/memory.x', 'r') as memscript:
            template = string.Template(memscript.read())

        contents = template.substitute(ram_length=ram_length,
                                       rom_length=rom_length,
                                       rom_origin=rom_origin)
        with open(mcu_ldscripts_path + '/memory.x', 'w') as memscript:
            memscript.write(contents)

    with open(template_path + '/msp430.x', 'r') as ldscript:
        template = string.Template(ldscript.read())
else:
    mcu_ldscripts_path = tmp_ldscripts_path
    with open(template_path + '/sancus.ld', 'r') as ldscript:
        template = string.Template(ldscript.read())

contents = template.substitute(sm_text_sections=text_sections,
                               sm_data_sections=data_sections,
                               sm_mac_sections=mac_sections,
                               sm_symbols=symbols,
                               mcu_ldscripts_path=mcu_ldscripts_path)

ldscript_name = tmp_ldscripts_path + '/msp430.x'
with open(ldscript_name, 'w') as ldscript:
    ldscript.write(contents)

#with open(ldscript_name, 'r') as ldscript:
    #print ldscript.read()

out_file = args.out_file
if not out_file:
    out_file = 'a.out'

info('Using output file ' + out_file)

ld_args = ['-L', mcu_ldscripts_path, '-L', msp_paths['lib'],
           '-L', sancus.paths.get_data_path() + '/lib',
           '-T', ldscript_name, '-o', out_file]
ld_libs = ['-lsancus-sm-support']

if args.standalone:
    ld_libs += ['-lsancus-host-support']
    ld_args += args.in_files + cli_ld_args + ld_libs
    call_prog('msp430-gcc', ld_args)
else:
    # -d makes sure no COMMON symbols are created since these are annoying to
    # handle in the dynamic loader (and pretty useless anyway)
    ld_args += ['-r', '-d'] + args.in_files + cli_ld_args + ld_libs
    call_prog('msp430-ld', ld_args)
