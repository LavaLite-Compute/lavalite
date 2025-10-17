#!/usr/bin/env perl
#
# strip_i18n.pl — Remove legacy _i18n_msg_get() / I18N_MSG() wrappers
# and useless #define NL_SETN lines from LavaLite source files.
#
# It replaces each call with its final argument (the actual message),
# removes trailing /* catgets NNN */ comments, and drops
#   #define NL_SETN <number>
# lines, while preserving layout.
#
# Usage:
#   ./strip_i18n.pl file.c [more files...]
#   find . -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 ./strip_i18n.pl
#

use strict;
use warnings;

# ---------- Utility ----------
sub read_whole {
    my ($path) = @_;
    open my $fh, '<', $path or die "Cannot open $path: $!";
    local $/;
    my $data = <$fh>;
    close $fh;
    return $data;
}

sub write_whole {
    my ($path, $data) = @_;
    open my $fh, '>', $path or die "Cannot write $path: $!";
    print {$fh} $data;
    close $fh;
}

# ---------- Parser helpers ----------
sub find_matching_paren {
    my ($s, $start) = @_;
    my ($i, $len, $depth) = ($start, length($s), 0);
    my ($in_s, $in_c, $in_lb, $in_bc) = (0,0,0,0);
    while ($i < $len) {
        my $ch = substr($s, $i, 1);
        my $n2 = substr($s, $i, 2);
        if (!$in_s && !$in_c && !$in_bc && $n2 eq '//'){ $in_lb=1;$i+=2;next;}
        if ($in_lb){ $i++ and $in_lb=0 if $ch eq "\n"; next; }
        if (!$in_s && !$in_c && !$in_bc && $n2 eq '/*'){ $in_bc=1;$i+=2;next;}
        if ($in_bc){ if($n2 eq '*/'){ $in_bc=0;$i+=2;next;} $i++;next;}
        if(!$in_c&&!$in_s&&$ch eq '"'){ $in_s=1;$i++;next;}
        if($in_s){ if($ch eq '\\'){ $i+=2;next;} if($ch eq '"'){ $in_s=0;$i++;next;} $i++;next;}
        if(!$in_s&&!$in_c&&$ch eq "'"){ $in_c=1;$i++;next;}
        if($in_c){ if($ch eq '\\'){ $i+=2;next;} if($ch eq "'"){ $in_c=0;$i++;next;} $i++;next;}
        if($ch eq '('){$depth++;} if($ch eq ')'){ $depth--; return $i if $depth==0;}
        $i++;
    }
    return -1;
}

sub split_top_level_commas {
    my ($s) = @_;
    my @parts; my $buf=''; my $len=length($s);
    my ($dp,$db,$dc,$in_s,$in_c,$in_lb,$in_bc)=(0,0,0,0,0,0,0);
    for (my $i=0;$i<$len;){
        my $ch=substr($s,$i,1); my $n2=substr($s,$i,2);
        if(!$in_s&&!$in_c&&!$in_bc&&$n2 eq '//'){ $in_lb=1;$buf.=$n2;$i+=2;next;}
        if($in_lb){$buf.=$ch; if($ch eq "\n"){$in_lb=0;} $i++;next;}
        if(!$in_s&&!$in_c&&!$in_bc&&$n2 eq '/*'){ $in_bc=1;$buf.=$n2;$i+=2;next;}
        if($in_bc){$buf.=$ch; if($n2 eq '*/'){ $buf.='/';$in_bc=0;$i+=2;next;} $i++;next;}
        if(!$in_c&&!$in_s&&$ch eq '"'){ $in_s=1;$buf.=$ch;$i++;next;}
        if($in_s){$buf.=$ch; if($ch eq '\\'){$buf.=substr($s,$i+1,1) if $i+1<$len;$i+=2;next;}
                   if($ch eq '"'){$in_s=0;$i++;next;} $i++;next;}
        if(!$in_s&&!$in_c&&$ch eq "'"){ $in_c=1;$buf.=$ch;$i++;next;}
        if($in_c){$buf.=$ch; if($ch eq '\\'){$buf.=substr($s,$i+1,1) if $i+1<$len;$i+=2;next;}
                   if($ch eq "'"){$in_c=0;$i++;next;} $i++;next;}
        if($ch eq '('){$dp++;} elsif($ch eq ')'){$dp--;}
        elsif($ch eq '['){$db++;} elsif($ch eq ']'){$db--;}
        elsif($ch eq '{'){$dc++;} elsif($ch eq '}'){$dc--;}
        if($ch eq ',' && !$dp && !$db && !$dc){ push @parts,$buf; $buf=''; $i++; next;}
        $buf.=$ch; $i++;
    }
    push @parts,$buf if $buf ne '';
    return @parts;
}

sub rewrite_once {
    my ($src) = @_;
    my $changed = 0;

    # Remove _i18n_msg_get() / I18N_MSG() wrappers
    for my $name (qw/_i18n_msg_get I18N_MSG/) {
        my $pos = 0;
        while ( (my $idx = index($src, $name, $pos)) >= 0 ) {
            my $open = index($src, '(', $idx + length($name));
            last if $open < 0;
            my $close = find_matching_paren($src, $open);
            last if $close < 0;
            my $args = substr($src, $open + 1, $close - $open - 1);
            my @args = split_top_level_commas($args);
            my $replacement = $args[-1] // '';
            substr($src, $idx, $close - $idx + 1, $replacement);
            $pos = $idx + length($replacement);
            $changed = 1;
        }
    }

    # Remove trailing /* catgets NNN */ comments
    my $c1 = ($src =~ s{[ \t]*\/\*\s*catgets\s*\d+\s*\*\/[ \t]*\r?\n}{\n}g);
    my $c2 = ($src =~ s{[ \t]*\/\*\s*catgets\s*\d+\s*\*\/}{ }g);

    # Remove useless defines like "#define NL_SETN 500"
    my $c3 = ($src =~ s/^[ \t]*#\s*define\s+NL_SETN\s+\S+\s*\r?\n//mg);

    $changed ||= ($c1 || $c2 || $c3);
    return ($src,$changed);
}

sub rewrite_all {
    my ($src) = @_;
    while (1) {
        my ($new,$changed)=rewrite_once($src);
        $src=$new;
        last unless $changed;
    }
    return $src;
}

# ---------- Main ----------
if (!@ARGV) {
    die "Usage: $0 <files...>\n";
}

for my $f (@ARGV) {
    my $orig = read_whole($f);
    my $new  = rewrite_all($orig);
    next if $new eq $orig;
    write_whole("$f.bak", $orig);
    write_whole($f, $new);
    print "Rewrote $f\n";
}

