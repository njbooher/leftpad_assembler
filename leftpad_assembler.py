#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function
import argparse
from collections import defaultdict
import sys

def rc(seq):
    complement = {'A': 'T', 'T': 'A', 'C': 'G', 'G': 'C', 'N': 'N'}
    return ''.join([complement[base] for base in reversed(seq)])

# https://github.com/lh3/readfq
def readfq(fp): # this is a generator function
    last = None # this is a buffer keeping the last unprocessed line
    while True: # mimic closure; is it a bad idea?
        if not last: # the first record or a record following a fastq
            for l in fp: # search for the start of the next record
                if l[0] in '>@': # fasta/q header line
                    last = l[:-1] # save this line
                    break
        if not last: break
        name, seqs, last = last[1:].partition(' ')[0], [], None
        for l in fp: # read the sequence
            if l[0] in '@+>':
                last = l[:-1]
                break
            seqs.append(l[:-1])
        if not last or last[0] != '+': # this is a fasta record
            yield name, ''.join(seqs), None # yield a fasta record
            if not last: break
        else: # this is a fastq record
            seq, leng, seqs = ''.join(seqs), 0, []
            for l in fp: # read the quality
                seqs.append(l[:-1])
                leng += len(l) - 1
                if leng >= len(seq): # have read enough quality
                    last = None
                    yield name, seq, ''.join(seqs); # yield a fastq record
                    break
            if last: # reach EOF before reading enough quality
                yield name, seq, None # yield a fasta record instead
                break

def main(input_file, anchor):

    read_pad = 0

    matching_reads = []

    for name, seq_str, qual in readfq(input_file):

        anchor_pos = seq_str.find(anchor)

        if anchor_pos != -1:
            matching_reads.append((anchor_pos, seq_str))
            read_pad = max([read_pad, anchor_pos])
        else:
            rev_seq_str = rc(seq_str)
            anchor_pos = rev_seq_str.find(anchor)
            if anchor_pos != -1:
                matching_reads.append((anchor_pos, rev_seq_str))
                read_pad = max([read_pad, anchor_pos])

    padded_reads = defaultdict(list)

    for (anchor_pos, seq_str) in matching_reads:
        aligned_read_start_pos = max([0, read_pad - anchor_pos])
        padded_reads[aligned_read_start_pos].append(' ' * aligned_read_start_pos + seq_str)

    print('Anchor: ')
    print(' ' * read_pad + anchor)
    print('Reads:')

    for start_pos in sorted(padded_reads, key=padded_reads.get, reverse=True):
        for item in sorted(padded_reads[start_pos]):
            print(item)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Next-generation genome assembler with unprecedented sensitivity and performance characterstics',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('anchor', help='A seed to align reads to')
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'), default=sys.stdin, help='A fasta or fastq file')
    args = parser.parse_args()

    main(args.infile, args.anchor)
