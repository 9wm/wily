# Routines for HTML to ASCII.
# (fixed width font, no font changes for size, bold, etc) with a little

# BUGS AND MISSING FEATURES
#  font tags (e.g. CODE, EM) cause an extra whitespace 
#   e.g. <TT>foo</TT>, -> foo ,

# Jim Davis July 15 1994
# modified 3 Aug 94 to support MENU and DIR

require "tformat.pl" || die "Could not load tformat.pl: $@\nStopped";

# Can be set by command line arg
if (! defined($columns_per_line)) {
    $columns_per_line = 72;}

if (! defined($flush_last_page)) {
    $flush_last_page = 1;}

# amount to add to indentation
$indent_left = 5;
$indent_right = 5;

# ignore contents inside HEAD.
$ignore_text = 0;

# Set variables in tformat
$left_margin = 1;
$right_margin = $columns_per_line;
$bottom_margin = 0;

## Routines called by html.pl
$Begin{"HEAD"} = "begin_head";
$End{"HEAD"} = "end_head";

sub begin_head {
    local ($element, $tag) = @_;
    $ignore_text = 1;}

sub end_head {
    local ($element) = @_;
    $ignore_text = 0;}

$Begin{"BODY"} = "begin_document";

sub begin_document {
    local ($element, $tag) = @_;
    &start_page();}

$End{"BODY"} = "end_document";

sub end_document {
    local ($element) = @_;
    &fresh_line();}

## Headers

$Begin{"H1"} = "begin_header";
$End{"H1"} = "end_header";

$Begin{"H2"} = "begin_header";
$End{"H2"} = "end_header";

$Begin{"H3"} = "begin_header";
$End{"H3"} = "end_header";

$Begin{"H4"} = "begin_header";
$End{"H4"} = "end_header";

$Skip_Before{"H1"} = 1;
$Skip_After{"H1"} = 1;

$Skip_Before{"H2"} = 1;
$Skip_After{"H2"} = 1;

$Skip_Before{"H3"} = 1;
$Skip_After{"H3"} = 0;

sub begin_header {
    local ($element, $tag) = @_;
    &skip_n_lines ($Skip_Before{$element}, 5);}

sub end_header {
    local ($element) = @_;
    &skip_n_lines ($Skip_After{$element});}

$Begin{"BR"} = "line_break";

sub line_break {
    local ($element, $tag) = @_;
    &fresh_line();}

$Begin{"P"} = "begin_paragraph";

# if fewer than this many lines left on page, start new page
$widow_cutoff = 5;

sub begin_paragraph {
    local ($element, $tag) = @_;
    &skip_n_lines (1, $widow_cutoff);}

$Begin{"BLOCKQUOTE"} = "begin_blockquote";
$End{"BLOCKQUOTE"} = "end_blockquote";

sub begin_blockquote {
    local ($element, $tag) = @_;
    $left_margin += $indent_left;
    $right_margin = $columns_per_line - $indent_right;
    &skip_n_lines (1);}

sub end_blockquote {
    local ($element) = @_;
    $left_margin -= $indent_left;
    $right_margin = $columns_per_line;
    &skip_n_lines (1);}

$Begin{"PRE"} = "begin_pre";
$End{"PRE"} = "end_pre";

sub begin_pre {
    local ($element, $tag) = @_;
    $whitespace_significant = 1;}

sub end_pre {
    local ($element) = @_;
    $whitespace_significant = 0;}

$Begin{"INPUT"} = "form_input";

sub form_input {
    local ($element, $tag, *attributes) = @_;
    if ($attributes{"value"} ne "") {
	&print_word_wrap($attributes{"value"});}}

$Begin{"HR"} = "horizontal_rule";

sub horizontal_rule {
    local ($element, $tag) = @_;
    &fresh_line ();
    &print_n_chars ($right_margin - $left_margin, "-");}

# Add code for IMG (use ALT attribute)
# Ignore I, B, EM, TT, CODE (no font changes)

## List environments

$Begin{"UL"} = "begin_itemize";
$End{"UL"} = "end_list_env";

$Begin{"OL"} = "begin_enumerated";
$End{"OL"} = "end_list_env";

$Begin{"MENU"} = "begin_menu";
$End{"MENU"} = "end_list_env";

$Begin{"DIR"} = "begin_dir";
$End{"DIR"} = "end_list_env";

$Begin{"LI"} = "begin_list_item";

# application-specific initialization routine
sub html_begin_doc {
    @list_stack = ();
    $list_type = "bullet";
    $list_counter = 0;}

sub push_list_env {
    push (@list_stack, join (":", $list_type, $list_counter));}

sub pop_list_env {
    ($list_type, $list_counter) = split (":", pop (@list_stack));
    $left_margin -= $indent_left;}

sub begin_itemize {
    local ($element, $tag) = @_;
    &push_list_env();
    $left_margin += $indent_left;
    $list_type = "bullet";
    $list_counter = "*";}

sub begin_menu {
    local ($element, $tag) = @_;
    &push_list_env();
    $left_margin += $indent_left;
    $list_type = "bullet";
    $list_counter = "*";}

sub begin_dir {
    local ($element, $tag) = @_;
    &push_list_env();
    $left_margin += $indent_left;
    $list_type = "bullet";
    $list_counter = "*";}

sub begin_enumerated {
    local ($element, $tag) = @_;
    &push_list_env();
    $left_margin += $indent_left;
    $list_type = "enumerated";
    $list_counter = 1;}

sub end_list_env {
    local ($element) = @_;
    &pop_list_env();
#    &fresh_line();
}

sub begin_list_item {
    local ($element, $tag) = @_;
    $left_margin -= 2;
    &fresh_line();
    &print_word_wrap("$list_counter ");
    if ($list_type eq "enumerated") {$list_counter++;}
    $left_margin += 2;}

$Begin{"DL"} = "begin_dl";

sub begin_dl {
    local ($element, $tag) = @_;
    &skip_n_lines(1,5);}
    
$Begin{"DT"} = "begin_defined_term";
$Begin{"DD"} = "begin_defined_definition";
$End{"DD"} = "end_defined_definition";

sub begin_defined_term {
    local ($element, $tag) = @_;
    &fresh_line();}

sub begin_defined_definition {
    local ($element, $tag) = @_;
    $left_margin += $indent_left;
    &fresh_line();}

sub end_defined_definition {
    local ($element) = @_;
    $left_margin -= $indent_left;
    &fresh_line();}

$Begin{"META"} = "begin_meta";

# a META tag sets a value in the assoc array %Variable
# i.e. <META name="author" content="Rushdie"> sers $Variable{author} to "Rushdie"
sub begin_meta {
    local ($element, $tag, *attributes) = @_;
    local ($variable, $value);
    $variable = $attributes{name};
    $value = $attributes{content};
    $Variable{$variable} = $value;}

$Begin{"IMG"} = "begin_img";

sub begin_img {
    local ($element, $tag, *attributes) = @_;
    &print_word_wrap (($attributes{"alt"} ne "") ? $attributes{"alt"} : "[IMAGE]");}

# URLs

$Begin{"A"} = "begin_a";
sub begin_a {
	local ($element, $tag, *attributes) = @_;
	local ($href, $k);
	$href = $attributes{href};
	$k = $main'wily_url++;
	$main'wily_urls{$k} = $href;
	&print_word_wrap ("[_u$k][");
}

$End{"A"} = "end_a";
sub end_a {
	&print_word_wrap("]");
}

# Content and whitespace.

sub html_content {
    local ($string) = @_;
    unless ($ignore_text) {
	&print_word_wrap ($string);}}

sub html_whitespace {
    local ($string) = @_;
    if (! $whitespace_significant) {
	die "Internal error, called html_whitespace when whitespace was not significant";}
    local ($i);
    for ($i = 0; $i < length ($string); $i++) {
	&print_whitespace (substr($string,$i,1));}}

# called by tformat.  Do nothing.
sub do_footer {
}

sub do_header {
}


1;
