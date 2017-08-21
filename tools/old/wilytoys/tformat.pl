# Simple text formatter
# Jim Davis 17 July 94

# current page, line, and column numbers.
$page = 1;
$line = 1;
$column = 1;

$left_margin = 1;
$right_margin = 72;
# lines on page before footer.  or 0 if no limit.
$bottom_margin = 58;

# add newlines to make page be full length?
$fill_page_length = 1;

sub print_word_wrap {
    local ($word) = @_;
    if (($column + ($whitespace_significant ? 0 : 1)
	 + length ($word) ) > ($right_margin + 1)) {
	&fresh_line();}
    if ($column > $left_margin && !$whitespace_significant) {
	print " ";
	$column++;}
    print $word;
    $column += length ($word);}
					 

sub print_whitespace {
    local ($char) = @_;
    if ($char eq " ") {
	$column++;
	print " ";}
    elsif ($char eq "\t") {
	&get_to_column (&tab_column($column));}
    elsif ($char eq "\n") {
	&new_line();}
    else {
	die "Unknown whitespace character \"$char\"\nStopped";}
    }

sub tab_column {
    local ($c) = @_;
    (int (($c-1) / 8) + 1) * 8 + 1;}

sub fresh_line {
    if ($column > $left_margin) {&new_line();}
    while ($column < $left_margin) {
	print " "; $column++;}}				 

sub finish_page {
    # Add extra newlines to finish page. 
    # You might not want to do this on the last page.
    if ($fill_page_length) {
	while ($line < $bottom_margin) {&cr();}}
    &do_footer ();
    $line = 1; $column = 1;}

sub start_page {
    if ($page != 1) {
	&do_header ();}}

sub print_n_chars {
    local ($n, $char) = @_;
    local ($i);
    for ($i = 1; $i <= $n; $i++) {print $char;}
    $column += $n;}

# need one NL to end current line, and then N to get N blank lines.
sub skip_n_lines {
    local ($n, $room_left) = @_;
    if ($bottom_margin > 0 && $line + $room_left >= $bottom_margin) {
	&finish_page();
	&start_page();}
    else {
	local ($i);
	for ($i = 0; $i <= $n; $i++) {&new_line();}}}

sub new_line {
    if ($bottom_margin > 0 && $line >= $bottom_margin) {
	&finish_page();
	&start_page();}
    else {&cr();}
    &print_n_chars ($left_margin - 1, " ");}

# used in footer and header where we don't respect the bottom margin.
sub print_blank_lines {
    local ($n) = @_;
    local ($i);
    for ($i = 0; $i < $n; $i++) {&cr();}}
    

sub cr {
    print "\n";
    $line++;
    $column = 1;}


# left, center, and right tabbed items

sub print_lcr_line {
    local ($left, $center, $right) = @_;
    &print_tab_left (1, $left);
    &print_tab_center (($right_margin - $left_margin) / 2, $center);
    &print_tab_right ($right_margin, $right);
    &cr();}

sub print_tab_left {
    local ($tab_column, $string) = @_;
    &get_to_column ($tab_column);
    print  $string;     $column += length ($string);
}

sub print_tab_center {
    local ($tab_column, $string) = @_;
    &get_to_column ($tab_column - (length($string) / 2));
    print $string;     $column += length ($string);
}

sub print_tab_right {
    local ($tab_column, $string) = @_;
    &get_to_column ($tab_column - length($string));
    print $string;
    $column += length ($string);
}


sub get_to_column {
    local ($goal_column) = @_;
    if ($column > $goal_column) {print " "; $column++;}
    else {
	while ($column < $goal_column)  {
	    print " "; $column++;}}}

