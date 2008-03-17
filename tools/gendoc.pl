#!/usr/bin/perl
#
#   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
#   (c) Copyright 2000-2004  Convergence (integrated media) GmbH
#
#   All rights reserved.
#
#   Written by Denis Oliver Kropp <dok@directfb.org>,
#              Andreas Hundt <andi@fischlustig.de>,
#              Sven Neumann <neo@directfb.org>,
#              Ville Syrjälä <syrjala@sci.fi> and
#              Claudio Ciccani <klan@users.sf.net>.
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
#
#   You should have received a copy of the GNU Lesser General Public
#   License along with this library; if not, write to the
#   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#   Boston, MA 02111-1307, USA.
#

##################################
# TODO: CLEANUP CODE FURTHER !!!
##################################

#####################################################################################
#                                                                                   #
#  Documentation generator written by Denis Oliver Kropp <dok@directfb.org>         #
#                                                                                   #
#  - Uses first argument as project name and second as version                      #
#  - Reads header files from stdin, parsing is tied to the coding style             #
#  - Writes HTML 3.x to different files: 'index', 'types', <interfaces>, <methods>  #
#                                                                                   #
#  FIXME: remove all copy'n'waste code, cleanup more, simplify more, ...            #
#                                                                                   #
#####################################################################################

$COLOR_BG                = "#F8F4D8";
$COLOR_LINK              = "#2369E0";
$COLOR_TEXT              = "#232323";

$COLOR_TOP_BG            = "#000000";
$COLOR_TOP_LINK          = "#FFFFFF";

$COLOR_TITLE             = "#CC7723";
$COLOR_TITLE_BG          = "#203040";
$COLOR_TITLE_MAIN        = "#DDDDDD";

$COLOR_ENTRIES_BG        = "#F8F8F0";
$COLOR_ENTRIES_PTR       = "#424242";
$COLOR_ENTRIES_ID        = "#234269";
$COLOR_ENTRIES_DESC      = "#303030";

$COLOR_ENUM_NAME         = "#B04223";
$COLOR_ENUM_ENTRY_ID     = "#429023";
$COLOR_ENUM_ENTRY_VAL    = "#234269";

$COLOR_STRUCT_NAME       = "#238423";

$COLOR_FUNCTYPE_NAME     = "#D06923";
$COLOR_FUNCTYPE_HEAD     = "#232342";

$COLOR_MACRO_NAME        = "#2342A0";
$COLOR_MACRO_PARAMS      = "#606080";
$COLOR_MACRO_VALUE       = "#232342";

$COLOR_METHOD_HEAD       = "#425469";

$COLOR_COPYRIGHT_BG      = "#E0E8F0";

########################################################################################################################
## Top level just calls main function with args
#

$PROJECT = shift @ARGV;
$VERSION = shift @ARGV;

gen_doc( $PROJECT, $VERSION );

########################################################################################################################

########################################################################################################################
## Utilities
#

sub trim ($) {
   local (*str) = @_;

   # remove leading white space
   $str =~ s/^\s*//g;

   # remove trailing white space and new line
   $str =~ s/\s*$//g;
}

sub print_list ($$) {
   local (*list, $title) = @_;

   print INDEX "<P>\n",
               "  <CENTER>\n",
               "    <H3>$title</H3>\n",
               "    <TABLE width=90% border=0 cellpadding=2>\n";

   foreach $key (sort keys %list)
      {
         print INDEX "    <TR><TD valign=top>\n",
                     "      <A href=\"types.html#$key\">$key</A>\n",
                     "    </TD><TD valign=top>\n",
                     "      $list{$key}\n",
                     "    </TD></TR>\n";
      }

   print INDEX "  </TABLE></CENTER>\n",
               "</P>\n";
}

sub substitute_links ($) {
   local (*str) = @_;

   # Interface Methods
   $str =~ s/(I\w+)\:\:(\w+)\(\)/\<a\ href=\"\1\_\2\.html\"\>\1\:\:\2\(\)\<\/a\>/g;

   # Automatic type links
   $str =~ s/(\s)([A-Z][A-Z][A-Z][A-Z]?[a-z][a-z][a-z]?[\w0-9]+)/\1\<a\ href=\"types\.html#\2\"\>\2\<\/a\>/g;

   # Automatic type links
   $str =~ s/(\s)($PROJECT[\w0-9]+)/\1\<a\ href=\"types\.html#\2\"\>\2\<\/a\>/g;

   # Explicit type links
   $str =~ s/(\s)\@\_(\w[\w0-9]+)/\1\<a\ href=\"types\.html#\2\"\>\2\<\/a\>/g;
}

sub type_link ($) {
   my ($type) = @_;

   trim( \$type );

   if (defined($type_list{$type}))
      {
         return "<A href=\"types.html#$type\">$type</A>";
      }
   elsif (defined($interfaces{$type}))
      {
         return "<A href=\"$type.html\">$type</A>";
      }

   return "$type";
}

########################################################################################################################
## Generic parsers
#

sub parse_comment ($$$$) {
   local (*head, *body, *options, $inithead) = @_;

   local $headline_mode = 1;
   local $list_open     = 0;

   $head = "\n";
   $body = "\n";

   if ($inithead ne "") {
      $headline_mode = 0;

      $head .= "        $inithead\n";
   }

   %options = ();

   while (<>)
      {
         chomp;
         last if /^\s*\*+\/\s*$/;

         # Prepend asterisk if first non-whitespace isn't an asterisk
         s/^\s*([^\*\s])/\* $1/;

         # In head line mode append to $head
         if ($headline_mode == 1)
            {
               if (/^\s*\*+\s*$/)
                  {
                     $headline_mode = 0;
                  }
               elsif (/^\s*\*+\s*@(\w+)\s*=?\s*(.*)$/)
                  {
                     $options{$1} = $2;
                  }
               elsif (/^\s*\*+\s*(.+)\*\/\s*$/)
                  {
                     $head .= "        $1\n";
                     last;
                  }
               elsif (/^\s*\*+\s*(.+)$/)
                  {
                     $head .= "        $1\n";
                  }
            }
         else
            # Otherwise append to $body
            {
               if (/^\s*\*+\s*(.+)\*\/\s*$/)
                  {
                     $body .= "        $1\n";
                     last;
                  }
               elsif (/^\s*\*+\s*$/)
                  {
                     $body .= " </P><P>\n";
                  }
               elsif (/^\s*\*+\s\-\s(.+)$/)
                  {
                     if ($list_open == 0)
                        {
                           $list_open = 1;

                           $body .= " <UL><LI>\n";
                        }
                     else
                        {
                           $body .= " </LI><LI>\n";
                        }

                     $body .= "        $1\n";
                  }
               elsif (/^\s*\*+\s\s(.+)$/)
                  {
                     $body .= "        $1\n";
                  }
               elsif (/^\s*\*+\s(.+)$/)
                  {
                     if ($list_open == 1)
                        {
                           $list_open = 0;

                           $body .= " </LI></UL>\n";
                        }

                     $body .= "        $1\n";
                  }
            }
      }

   if ($list_open == 1)
      {
         $body .= " </LI></UL>\n";
      }

   substitute_links (\$head);
   substitute_links (\$body);
}

#
# Reads stdin until the end of the parameter list is reached.
# Returns list of parameter records.
#
# TODO: Add full comment support and use it for function types as well.
#
sub parse_params () {
   local @entries;

   while (<>)
      {
         chomp;
         last if /^\s*\)\;\s*$/;

         if ( /^\s*(const )?\s*([\w\ ]+)\s+(\**)(\w+,?)\s*$/ )
            {
               local $const = $1;
               local $type  = $2;
               local $ptr   = $3;
               local $name  = $4;

               local $rec = {
                  TYPE   => $const . type_link( $type ),
                  PTR    => $ptr,
                  NAME   => $name
               };

               push (@entries, $rec);
            }
      }

   return @entries;
}

########################################################################################################################
## Type parsers
#

#
# Reads stdin until the end of the interface is reached.
# Writes formatted HTML to one file for the interface and one file per method.
# Parameter is the interface name.
#
sub parse_interface ($)
   {
      local ($interface) = @_;

      local $section;

      trim( \$interface );

      if (!defined ($interfaces{$interface})) {
         print "WARNING: Interface definition '$interface' has no declaration!\n"
      }

      html_create( INTERFACE, "$interface.html",
                              "<A href=\"index.html\">" .
                              "  <FONT color=$COLOR_TITLE_MAIN>$PROJECT Interfaces</FONT>" .
                              "</A>", $interface, $interface );

      print INTERFACE "<P>\n",
                      "  $headline\n",
                      "  $detailed\n",
                      "</P>";

      print INTERFACE "<P>\n",
                      "  <CENTER><TABLE width=93% border=1 rules=groups cellpadding=4 cellspacing=2>\n";

      print INTERFACE "    <THEAD>\n";
      print INTERFACE "      <TR><TH colspan=3>Methods of $interface</TH></TR>\n";
      print INTERFACE "    </THEAD>\n";

      print INTERFACE "    <TBODY>\n";

      while (<>)
         {
            chomp;
            last if /^\s*\)\s*$/;

            if ( /^\s*\/\*\*\s*(.+)\s*\*\*\/\s*$/ )
               {
                  $section = $1;
               }
            elsif ( /^\s*(\w+)\s*\(\s*\*\s*(\w+)\s*\)\s*\(?\s*$/ )
               {
                  print INTERFACE "    <TR><TD valign=top>\n",
                                  "      <B><SMALL>$section</SMALL></B>\n",
                                  "    </TD><TD valign=top>\n",
                                  "      <A href=\"${interface}_$2.html\">",
                                  "      <B>$2</B></A>\n",
                                  "    </TD><TD valign=top>\n",
                                  "      $headline\n",
                                  "    </TD></TR>\n";

                  html_create( FUNCTION, "${interface}_$2.html",
                               "<A href=\"$interface.html\">" .
                               "  <FONT color=$COLOR_TITLE_MAIN>$interface</FONT>" .
                               "</A>", $2, "$interface - $2" );

                  print FUNCTION "<H4>$headline</H4>\n",
                                 "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=$COLOR_ENTRIES_BG>\n",
                                 "    <TR><TD colspan=5><I><FONT color=$COLOR_METHOD_HEAD><A href=\"types.html#$1\">$1</A> <b>$2 (</b></FONT></I></TD></TR>\n";

                  local @params = parse_params();
                  local $param;

                  for $param (@params)
                     {
                        print FUNCTION "    <TR><TD width=50>\n",
                                       "      &nbsp;\n",
                                       "    </TD><TD valign=top>\n",
                                       "      $param->{TYPE}\n",
                                       "    </TD><TD width=20>&nbsp;</TD><TD align=right>\n",
                                       "      <FONT color=$COLOR_ENTRIES_PTR><B>$param->{PTR}</B></FONT>\n",
                                       "    </TD><TD valign=top>\n",
                                       "      <FONT color=$COLOR_ENTRIES_ID><B>$param->{NAME}</B></FONT>\n",
                                       "    </TD></TR>\n";
                     }

                  print FUNCTION "    <TR><TD colspan=5><I><FONT color=$COLOR_METHOD_HEAD><b>);</b></FONT></I></TD></TR>\n",
                                 "  </TABLE>\n",
                                 "</P>\n";

                  print FUNCTION "<P>$detailed</P>\n";

                  $headline = "";
                  $detailed = "";
                  $section = "";

                  html_close( FUNCTION );
               }
            elsif ( /^\s*\/\*\s*$/ )
               {
                  parse_comment( \$headline, \$detailed, \$options, "" );
               }
         }

      print INTERFACE "    </TBODY>\n";

      print INTERFACE "  </TABLE></CENTER>\n",
                      "</P>\n";

      html_close( INTERFACE );
   }

#
# Reads stdin until the end of the enum is reached.
# Writes formatted HTML to "types.html".
#
sub parse_enum
   {
      local %entries;
      local @list;

      local $pre;

      while (<>)
         {
            chomp;

            local $entry;

            # entry with assignment (complete comment)
            if ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\|\!\s]+[^\,\s])\s*,?\s*\/\*\s*(.+)\s*\*\/\s*$/ )
               {
                  $entry = $1;
                  $values{ $entry } = $2;
                  $entries{ $entry } = $3;
               }
            # entry with assignment (opening comment)
            elsif ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\|\!\s]+[^\,\s])\s*,?\s*\/\*\s*(.+)\s*$/ )
               {
                  $entry = $1;
                  $values{ $entry } = $2;

                  parse_comment( \$t1, \$t2, \$opt, $3 );

                  $entries{ $entry } = $t1.$t2;
               }
            # entry with assignment (none or preceding comment)
            elsif ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\|\!\s]+[^\,\s])\s*,?\s*$/ )
               {
                  $entry = $1;
                  $values{ $entry } = $2;
                  $entries{ $entry } = $pre;
               }
            # entry without assignment (complete comment)
            elsif ( /^\s*(\w+)\s*,?\s*\/\*\s*(.+)\s*\*\/\s*$/ )
               {
                  $entry = $1;
                  $entries{ $entry } = $2;
               }
            # entry without assignment (opening comment)
            elsif ( /^\s*(\w+)\s*,?\s*\/\*\s*(.+)\s*$/ )
               {
                  $entry = $1;

                  parse_comment( \$t1, \$t2, \$opt, $2 );

                  $entries{ $entry } = $t1.$t2;
               }
            # entry without assignment (none or preceding comment)
            elsif ( /^\s*(\w+)\s*,?\s*$/ )
               {
                  $entry = $1;
                  $entries{ $entry } = $pre;
               }
            # preceding comment (complete)
            elsif ( /^\s*\/\*\s*(.+)\s*\*\/\s*$/ )
               {
                  $pre = $1;
               }
            # preceding comment (opening)
            elsif ( /^\s*\/\*\s*(.+)\s*$/ )
               {
                  parse_comment( \$t1, \$t2, \$opt, $1 );

                  $pre = $t1.$t2;
               }
            # end of enum
            elsif ( /^\s*\}\s*(\w+)\s*\;\s*$/ )
               {
                  $enum = $1;

                  trim( \$enum );

                  $enum_list{$enum} = $headline;
                  $type_list{$enum} = $headline;

                  last;
               }
            # blank line?
            else
               {
                  $pre = "";
               }

            if ($entry ne "")
               {
                  push (@list, $entry);
               }
         }

      if (scalar @list > 0)
         {
            print TYPES "<p>\n",
                        "  <a name=\"$enum\" href=\"#$enum\">\n",
                        "    <h3><font color=$COLOR_ENUM_NAME>$enum</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=$COLOR_ENTRIES_BG>\n";

            foreach $key (@list)
               {
                  substitute_links (\$entries{$key});

                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=$COLOR_ENUM_ENTRY_ID><b>$key</b></font>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=$COLOR_ENUM_ENTRY_VAL>$values{$key}</font>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=$COLOR_ENTRIES_DESC>$entries{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "</p><p>\n",
                        "  $detailed\n",
                        "</p><hr>\n";
         }
   }

#
# Reads stdin until the end of the enum is reached.
# Writes formatted HTML to "types.html".
#
sub parse_struct
   {
      local @entries;
      local %entries_params;
      local %entries_types;
      local %entries_ptrs;

      while (<>)
         {
            chomp;

            local $entry;

            # without comment
            if ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?;\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;
                  $text = "";
               }
            # complete one line entry
            elsif ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?;\s*\/\*\s*(.+)\*\/\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;
                  $text = $6;
               }
            # with comment opening
            elsif ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?;\s*\/\*\s*(.+)\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;

                  parse_comment( \$t1, \$t2, \$opt, $6 );

                  $text = $t1.$t2;
               }
            elsif ( /^\s*\}\s*(\w+)\s*\;\s*$/ )
               {
                  $struct = $1;

                  trim( \$struct );

                  $struct_list{$struct} = $headline;
                  $type_list{$struct} = $headline;

                  last;
               }

            if ($entry ne "")
               {
                  # TODO: Use structure
                  $entries_types{$entry} = $const . type_link( $type );
                  $entries_ptrs{$entry} = $ptr;
                  $entries_params{$entry} = $text;

                  push (@entries, $entry);
               }
         }

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=\"$struct\" href=\"#$struct\">\n",
                        "    <h3><font color=$COLOR_STRUCT_NAME>$struct</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=$COLOR_ENTRIES_BG>\n";

            foreach $key (@entries)
               {
                  substitute_links (\$entries_params{$key});

                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top align=right>\n",
                              "      <FONT color=$COLOR_ENTRIES_PTR>$entries_ptrs{$key}</FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=$COLOR_ENTRIES_ID><B>$key</B></FONT>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=$COLOR_ENTRIES_DESC>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "</p><p>\n",
                        "  $detailed\n",
                        "</p><hr>\n";
         }
   }

#
# Reads stdin until the end of the function type is reached.
# Writes formatted HTML to "types.html".
# Parameters are the return type and function type name.
#
sub parse_func ($$)
   {
      local ($rtype, $name) = @_;

      local @entries;
      local %entries_params;
      local %entries_types;
      local %entries_ptrs;

      trim( \$rtype );
      trim( \$name );

      while (<>)
         {
            chomp;

            local $entry;

            # without comment
            if ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?,?\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;
                  $text = "";
               }
            # complete one line entry
            elsif ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?,?\s*\/\*\s*(.+)\*\/\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;
                  $text = $6;
               }
            # with comment opening
            elsif ( /^\s*(const )?\s*([\w ]+)\s+(\**)([\w\d\+\[\]]+)(\s*:\s*\d+)?,?\s*\/\*\s*(.+)\s*$/ )
               {
                  $const = $1;
                  $type = $2;
                  $ptr = $3;
                  $entry = $4.$5;

                  parse_comment( \$t1, \$t2, \$opt, $6 );

                  $text = $t1.$t2;
               }
            elsif ( /^\s*\)\;\s*$/ )
               {
                  $func_list{$name} = $headline;
                  $type_list{$name} = $headline;

                  last;
               }

            if ($entry ne "")
               {
                  # TODO: Use structure
                  $entries_types{$entry} = $const . type_link( $type );
                  $entries_ptrs{$entry} = $ptr;
                  $entries_params{$entry} = $text;

                  push (@entries, $entry);
               }
         }

      $rtype = type_link( $rtype );

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=\"$name\" href=\"#$name\">\n",
                        "    <h3><font color=$COLOR_FUNCTYPE_NAME>$name</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=$COLOR_ENTRIES_BG>\n",
                        "    <TR><TD colspan=4>\n",
                        "         <I>$rtype (*<FONT color=$COLOR_FUNCTYPE_HEAD>$name</FONT>) (</I>\n",
                        "    </TD></TR>\n";

            foreach $key (@entries)
               {
                  print TYPES "    <TR><TD width=32>\n",
                              "      &nbsp;\n",
                              "    </TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top align=right>\n",
                              "      <FONT color=$COLOR_ENTRIES_PTR>$entries_ptrs{$key}</FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=$COLOR_ENTRIES_ID><B>$key</B></FONT>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=$COLOR_ENTRIES_DESC>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "    <TR><TD colspan=4><I>);</I></TD></TR>\n",
                        "  </TABLE>\n",
                        "</p><p>\n",
                        "  $detailed\n",
                        "</p><hr>\n";
         }
   }

#
# Reads stdin until the end of the macro is reached.
# Writes formatted HTML to "types.html".
# Parameters are the macro name, parameters and value.
#
sub parse_macro ($$$)
   {
      local ($macro, $params, $value) = @_;

      trim( \$macro );
      trim( \$params );
      trim( \$value );

      while (<>)
         {
            chomp;

            last unless /\\$/;
         }

      if (!defined ($options{"internal"}) && $value ne "") {
         $macro_list{$macro} = $headline;
         $type_list{$macro} = $headline;

         $value =~ s/^\s*\\\s*$//;

         print TYPES "<p>\n",
                     "  <a name=\"$macro\" href=\"#$macro\">\n",
                     "    <h3>\n",
                     "        <font color=$COLOR_MACRO_NAME>$macro</font>\n",
                     "        <font color=$COLOR_MACRO_PARAMS>$params</font>\n",
                     "    </h3>\n",
                     "  </a>\n",
                     "  <h4>$headline</h4>\n",
                     "  <font color=$COLOR_MACRO_VALUE size=+1><b>$value</b></font>\n",
                     "</p><p>\n",
                     "  $detailed\n",
                     "</p><hr>\n";
      }
   }

########################################################################################################################
## HTML Files
#

sub html_create ($$$$$)
   {
      local ($FILE, $filename, $title, $subtitle, $singletitle) = @_;

      open( $FILE, ">$filename" )
          or die ("*** Can not open '$filename' for writing:\n*** $!");

      print $FILE "<HTML>\n",
                  "<STYLE>\n",
                  "  <!--\n",
                  "    A{textdecoration:none}\n",
                  "  -->\n",
                  "</STYLE>\n",
                  "<STYLE type=\"text/css\">\n",
                  "  A:link, A:visited, A:active { text-decoration: none; }\n",
                  "</STYLE>\n",
                  "<HEAD>\n",
                  "  <TITLE>$singletitle [$PROJECT Reference Manual]</TITLE>\n",
                  "</HEAD>\n",
                  "<BODY bgcolor=$COLOR_BG link=$COLOR_LINK vlink=$COLOR_LINK text=$COLOR_TEXT>\n",
                  "\n",
                  "<TABLE width=100% bgcolor=$COLOR_TOP_BG border=0 cellspacing=1 cellpadding=3>\n",
                  "  <TR><TD width=30%>\n",
                  "    <A href=\"http://www.directfb.org\"><IMG border=0 src=\"directfb.png\"></A>\n",
                  "  </TD><TD align=right>\n",
                  "    &nbsp;&nbsp;",
                  "    <A href=\"index.html\"><FONT size=+3 color=$COLOR_TOP_LINK>Reference Manual - $VERSION</FONT></A>\n",
                  "  </TD></TR>\n",
                  "  <TR><TD colspan=2 align=center bgcolor=$COLOR_TITLE_BG>\n";

      if ($subtitle)
         {
            print $FILE "    <TABLE border=0 cellspacing=0 cellpadding=0>\n",
                        "      <TR><TD nowrap align=right width=50%>\n",
                        "        <BIG>$title&nbsp;</BIG>\n",
                        "      </TD><TD nowrap align=left width=50%>\n",
                        "        <BIG><FONT color=$COLOR_TITLE>&nbsp;$subtitle</FONT></BIG>\n",
                        "      </TD></TR>\n",
                        "    </TABLE>\n";
         }
      else
         {
            print $FILE "    <BIG><FONT color=$COLOR_TITLE>$title</FONT></BIG>\n";
         }

      print $FILE "  </TD></TR>\n",
                  "</TABLE>\n",
                  "\n";
   }

sub html_close ($)
   {
      local ($FILE) = @_;

      print $FILE "\n",
                  "<TABLE width=100% bgcolor=$COLOR_COPYRIGHT_BG border=0 cellspacing=1 cellpadding=3>\n",
                  "  <TR><TD width=100>\n",
                  "    <a rel=\"license\" href=\"http://creativecommons.org/licenses/by-sa/3.0/\">",
                  "    <img alt=\"Creative Commons License\" style=\"border-width:0\" border=\"0\" ",
                  "    src=\"http://i.creativecommons.org/l/by-sa/3.0/88x31.png\" />",
                  "    </a>",
                  "  </TD><TD>\n",
                  "    This work is licensed under a",
                  "    <a rel=\"license\" href=\"http://creativecommons.org/licenses/by-sa/3.0/\">",
                  "    Creative Commons Attribution-Share Alike 3.0 License</a>",
                  "  </TD></TR>\n",
                  "</TABLE>\n",
                  "</BODY>\n",
                  "</HTML>\n";

      close( $FILE );
   }


########################################################################################################################
## Main Function
#

sub gen_doc ($$) {
   local ($project, $version) = @_;

   trim( \$project );
   trim( \$version );

   html_create( INDEX, "index.html", "Index Page", "", "Index" );
   html_create( TYPES, "types.html", "$PROJECT Types", "", "Types" );
   
   print INDEX "<P>\n",
               "  <CENTER>\n",
               "    <H3>Interfaces</H3>\n",
               "    <TABLE width=90% border=0 cellpadding=2>\n";
   
   while (<>) {
      chomp;
   
      if ( /^\s*DECLARE_INTERFACE\s*\(\s*(\w+)\s\)\s*$/ ) {
         $interface = $1;

         trim( \$interface );

         if (!defined ($interfaces{$interface})) {
            print INDEX "    <TR><TD valign=top>\n",
                        "      <A href=\"$1.html\">$1</A>\n",
                        "    </TD><TD valign=top>\n",
                        "      $headline $detailed\n",
                        "    </TD></TR>\n";

            $interfaces{$interface} = "$headline $detailed";
         }
      }
      elsif ( /^\s*DEFINE_INTERFACE\s*\(\s*(\w+),\s*$/ ) {
         parse_interface( $1 );
      }
      elsif ( /^\s*typedef\s+enum\s*\{?\s*$/ ) {
         parse_enum();
      }
      elsif ( /^\s*typedef\s+(struct|union)\s*\{?\s*$/ ) {
         parse_struct();
      }
      elsif ( /^\s*typedef\s+(\w+)\s+\(\*(\w+)\)\s*\(\s*$/ ) {
         parse_func( $1, $2 );
      }
      elsif ( /^\s*#define\s+([^\(\s]+)(\([^\)]*\))?\s*(.*)/ ) {
         parse_macro( $1, $2, $3 );
      }
      elsif ( /^\s*\/\*\s*$/ ) {
         parse_comment( \$headline, \$detailed, \$options, "" );
      }
      else {
         $headline = "";
         $detailed = "";
         %options  = ();
      }
   }
   
   print INDEX "  </TABLE></CENTER>\n",
               "</P>\n";
   
   print_list( \%func_list, "Function Types" );
   print_list( \%enum_list, "Enumerated Types" );
   print_list( \%struct_list, "Structured Types" );
   print_list( \%macro_list, "Definitions" );
   
   
   html_close( INDEX );
   html_close( TYPES );
}

