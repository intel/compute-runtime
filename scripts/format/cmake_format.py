#!/usr/bin/env python3

#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

import sys
import argparse
import re


def remove_end_args(line):
    if line.startswith('endif('):
        line = re.sub(r'endif\(.*\)', 'endif()', line)
    elif line.startswith('endforeach('):
        line = re.sub(r'endforeach\(.*\)', 'endforeach()', line)
    elif line.startswith('endmacro('):
        line = re.sub(r'endmacro\(.*\)', 'endmacro()', line)
    elif line.startswith('endfunction('):
        line = re.sub(r'endfunction\(.*\)', 'endfunction()', line)

    return line


def remove_extra_spaces(line):
    line = re.sub(r' +', ' ', line)
    line = re.sub(r' *\( *', '(', line)
    line = re.sub(r' *\) *', ')', line)
    line = re.sub(r'\)AND\(', ') AND (', line)
    line = re.sub(r'\)OR\(', ') OR (', line)
    line = re.sub(r'NOT\(', 'NOT (', line)
    line = re.sub(r' *\) *(?=[A-Z])', ') ', line)
    return line


def process_line(line):
    split = line.split('"')
    opening_bracket_count = 0
    closing_bracket_count = 0
    new_line = []
    is_string = False
    is_first_part = True
    for l in split:
        if not is_string:
            l = replace_tabs(l)
            l = remove_extra_spaces(l)
            l = remove_end_args(l)
            if not is_first_part or not l.startswith('#'):
                l = re.sub(r' *#', ' #', l)
            opening_bracket_count += l.count('(')
            closing_bracket_count += l.count(')')
            new_line.append(l)
            is_string = True
        else:
            new_line.append(l)
            if not l.endswith('\\') or l.endswith('\\\\'):
                is_string = False

        is_first_part = False

    return '"'.join(new_line), opening_bracket_count, closing_bracket_count


def replace_tabs(line):
    return line.replace('\t', ' ')


def format_file(file):
    indent_size = 2
    indent_depth = 0
    extra_indent = ''
    previous_is_new_line = False
    lines = None
    with open(file) as fin:
        lines = fin.readlines()

    with open(file, 'w') as fout:
        for line in lines:
            indent = ''
            line = line.strip()
            if line.startswith('#'):
                indent = ' ' * indent_size * indent_depth
                fout.write(f'{indent}{extra_indent}{line}\n')
                previous_is_new_line = False
                continue

            line, opening_bracket_count, closing_bracket_count = process_line(
                line)
            if line.startswith('endif('):
                indent_depth -= 1
            elif line.startswith('else('):
                indent_depth -= 1
            elif line.startswith('elseif('):
                indent_depth -= 1
            elif line.startswith('endforeach('):
                indent_depth -= 1
            elif line.startswith('endmacro('):
                indent_depth -= 1
            elif line.startswith('endfunction('):
                indent_depth -= 1

            if line:
                indent = ' ' * indent_size * indent_depth
                previous_is_new_line = False
            else:
                if not previous_is_new_line:
                    fout.write('\n')

                previous_is_new_line = True
                continue

            if closing_bracket_count > opening_bracket_count:
                if not line.startswith(')'):
                    line = line.replace(')', f'\n{indent})', 1)
                    line = f'{indent}{extra_indent}{line}'
                    indent = ''

                extra_indent = ''

            fout.write(f'{indent}{extra_indent}{line}\n')

            if line.startswith('if('):
                indent_depth += 1
            elif line.startswith('else('):
                indent_depth += 1
            elif line.startswith('elseif('):
                indent_depth += 1
            elif line.startswith('foreach('):
                indent_depth += 1
            elif line.startswith('macro('):
                indent_depth += 1
            elif line.startswith('function('):
                indent_depth += 1

            if opening_bracket_count > closing_bracket_count:
                extra_indent = ' ' * \
                    len(re.match(r'.*\(', line).group(0))


def _parse_args():
    parser = argparse.ArgumentParser(
        description='Usage: ./scripts/format/cmake_format.py <files>')
    parser.add_argument('files', nargs='*')
    args = parser.parse_args()

    return vars(args)


def main(args):
    for file in args['files']:
        format_file(file)

    return 0


if __name__ == '__main__':
    sys.exit(main(_parse_args()))
