#!/usr/bin/perl
#
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Indents the lines that continue a command of a Doxygen block, \param,
# \return, \pre and the others of docs/Documentation.md, so that they start
# where the text after the command starts:
#
#    * \return The view of them, or IndexError::kOutOfRange if `position` lies
#    *         beyond the end.
#
# clang-format keeps such a line as it is, but when it breaks one that has
# grown too long, the new line starts at the margin again; so tools/lint.sh
# runs this after clang-format with --fix, and with --check fails where a
# line is not indented so. The paragraph after a command is rewrapped to 80
# columns; \code ... \endcode and the lines of a list are left as they are.
#
# Usage:
#   tools/doc_indent.pl --fix FILE...
#   tools/doc_indent.pl --check FILE...

use strict;
use warnings;

my $WIDTH = 80;
my $COMMAND = qr/\\(?:param(?:\[[a-z,]+\])?[ ]+\S+|tparam[ ]+\S+|retval[ ]+\S+
                    |return|pre|post|note|warning|stddiff|see|brief)/x;

# Returns the lines of a file with every command's paragraph indented.
sub Indented {
  my @lines = @_;
  my @out;
  my $in_block = 0;
  my $in_code = 0;
  for (my $i = 0; $i < @lines; $i++) {
    my $line = $lines[$i];
    $in_block = 1 if $line =~ m{^\s*/\*!(\s|$)} && $line !~ m{\*/};
    if (!$in_block) {
      push @out, $line;
      next;
    }
    if ($line =~ m{^\s*\*/}) {
      $in_block = 0;
      push @out, $line;
      next;
    }
    $in_code = 1 if $line =~ /\\code/;
    if ($in_code) {
      $in_code = 0 if $line =~ /\\endcode/;
      push @out, $line;
      next;
    }
    if ($line !~ /^(\s*)\* ($COMMAND)[ ]+(\S.*)$/) {
      push @out, $line;
      next;
    }
    my ($margin, $command, $text) = ($1, $2, $3);
    my $paragraph = $text;
    # The paragraph goes on to an empty line, the next command, a list or the
    # end of the block.
    my $j = $i + 1;
    while ($j < @lines && $lines[$j] =~ /^\s*\*[ ]+(\S.*)$/
           && $lines[$j] !~ /^\s*\*[ ]+(\\|-[ ])/ && $lines[$j] !~ m{^\s*\*/}) {
      $paragraph .= " $1";
      $j++;
    }
    # A code span in backticks is one word, never broken across lines.
    my @words = $paragraph =~ /((?:`[^`]*`|[^\s`])+)/g;
    my $first = "$margin* $command ";
    my $continued = "$margin* " . (' ' x (length($command) + 1));
    my $current = $first;
    my $empty = 1;
    for my $word (@words) {
      if (!$empty && length($current) + 1 + length($word) > $WIDTH) {
        push @out, "$current\n";
        $current = $continued . $word;
      } else {
        $current .= ($empty ? '' : ' ') . $word;
      }
      $empty = 0;
    }
    push @out, "$current\n";
    $i = $j - 1;
  }
  return @out;
}

sub Main {
  my $mode = shift @ARGV // '';
  if ($mode ne '--fix' && $mode ne '--check') {
    print STDERR "Usage: $0 --fix|--check FILE...\n";
    return 2;
  }
  my $failed = 0;
  for my $file (@ARGV) {
    open my $in, '<', $file or die "$file: $!";
    my @lines = <$in>;
    close $in;
    my @indented = Indented(@lines);
    next if join('', @lines) eq join('', @indented);
    if ($mode eq '--fix') {
      open my $out, '>', $file or die "$file: $!";
      print $out @indented;
      close $out;
    } else {
      print "$file: a line that continues a Doxygen command is not indented"
          . " under its text; tools/lint.sh --fix indents it\n";
      $failed = 1;
    }
  }
  return $failed;
}

exit Main();
