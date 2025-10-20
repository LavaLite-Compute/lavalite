#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long qw(GetOptions);
use File::Find ();

my ($dry, $verbose) = (0, 0);
GetOptions(
  'dry-run|n!' => \$dry,
  'verbose|v!' => \$verbose,
) or die "Usage: $0 [--dry-run|-n] [--verbose|-v] [FILES or DIRS...]\n";

my @targets;
sub add_if_source {
  my ($p) = @_;
  return unless -f $p;
  return unless $p =~ /\.(?:c|h)$/;
  push @targets, $p;
}
if (@ARGV) {
  for my $arg (@ARGV) {
    if (-d $arg) { File::Find::find({ wanted => sub { add_if_source($File::Find::name) } }, $arg) }
    elsif (-f $arg) { add_if_source($arg) }
    else { warn "skip: $arg (not found)\n" }
  }
} else {
  File::Find::find({ wanted => sub { add_if_source($File::Find::name) } }, '.');
}

my ($scanned, $changed) = (0, 0);

FILE: for my $f (@targets) {
  $scanned++;
  open my $in, '<', $f or (warn "read $f: $!" and next FILE);
  local $/; my $orig = <$in>; close $in;
  my $s = $orig;

  # I18N_m(id,"text") -> "text"
  $s =~ s{
    \bI18N_m\s*
    \(
      \s*[^,]*\s*,
      \s*(" (?:[^"\\]|\\.)* ")
    \s*
    \)
  }{$1}xg;

  # I18N(id,"text")   -> "text"
  $s =~ s{
    \bI18N\s*
    \(
      \s*[^,]*\s*,
      \s*(" (?:[^"\\]|\\.)* ")
    \s*
    \)
  }{$1}xg;

  # _i18n_msg_get(..., def)      -> def
  $s =~ s{
    \b_i18n_msg_get\s*
    \(
      [^,]*,\s*[^,]*,\s*[^,]*,\s*([^)]*)
    \)
  }{$1}xg;

  # _i18n_msgArray_get(..., def) -> def
  $s =~ s{
    \b_i18n_msgArray_get\s*
    \(
      [^,]*,\s*[^,]*,\s*[^,]*,\s*([^)]*)
    \)
  }{$1}xg;

  # init/teardown wrappers
  $s =~ s/\b_i18n_init\s*\([^)]*\)/0/g;
  $s =~ s/\b_i18n_end\s*\([^)]*\)/((void)0)/g;
  $s =~ s/\b_i18n_catopen\s*\([^)]*\)/0/g;
  $s =~ s/\b_i18n_catclose\s*\([^)]*\)/((void)0)/g;

  # remove NLS headers
  $s =~ s/^[ \t]*#\s*include\s*<\s*nl_types\.h\s*>[ \t]*\r?\n//mg;
  $s =~ s/^[ \t]*#\s*include\s*"\s*nl_types\.h\s*"[ \t]*\r?\n//mg;
  $s =~ s/^[ \t]*#\s*include\s*"\s*i18n\.h\s*"[ \t]*\r?\n//mg;

  # NL_SETN & ls_catd remnants
  $s =~ s/^[ \t]*#\s*define\s+NL_SETN\b[^\r\n]*\r?\n//mg;
  $s =~ s/\bNL_SETN\b/0/g;
  $s =~ s/^[ \t]*extern[ \t]+[A-Za-z_]\w*(?:[ \t]\*+)?[ \t]+ls_catd\s*;\s*\r?\n//mg;
  $s =~ s/^[ \t]*[A-Za-z_]\w*(?:[ \t]\*+)?[ \t]+ls_catd\s*;\s*\r?\n//mg;

  # kill /* ... catgets ... */ comments
  $s =~ s{/\*[^*]*catgets.*?\*/}{}sg;

  # remove standalone '((void)0);' lines and before-'}' variant
  $s =~ s/^\s*\(\(void\)\s*0\);\s*\r?\n//mg;
  $s =~ s/\s*\(\(void\)\s*0\);\s*(\})/$1/g;

  # minimal whitespace (NO semicolon or paren edits!)
  $s =~ s/[ \t]+$//mg;
  $s =~ s/\n{3,}/\n\n/g;

  # _i18n_ctime(catd, fmt, &var) -> ctime(&var)
  $s =~ s/\b_i18n_ctime\s*\([^,]*,\s*[^,]*,\s*(&?\w+)\s*\)/ctime($1)/g;

  next FILE if $s eq $orig;

  if ($dry) {
    print "[DRY] Would rewrite $f\n";
  } else {
    open my $out, '>', $f or (warn "write $f: $!" and next FILE);
    print {$out} $s; close $out;
    print "Rewrote $f\n" if $verbose || !$dry;
  }
  $changed++;
}

print "Done. Files scanned: $scanned, files changed: $changed\n";
exit 0;

