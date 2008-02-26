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

$PROJECT = shift @ARGV;
$VERSION = shift @ARGV;


# TODO: add more constants
$COLOR_LINK = "#2369E0";
$COLOR_TEXT = "#232323";


html_create( INDEX, "index.html", "Index Page", "", "Index" );
html_create( TYPES, "types.html", "$PROJECT Types", "", "Types" );

print INDEX "<P>\n",
            "  <CENTER>\n",
            "    <H3>Interfaces</H3>\n",
            "    <TABLE width=90% border=0 cellpadding=2>\n";

while (<>) {
   chomp;

   if ( /^\s*DECLARE_INTERFACE\s*\(\s*(\w+)\s\)\s*$/ ) {
      $interface_abstracts{$1} = "$headline $detailed";

      print INDEX "    <TR><TD valign=top>\n",
                  "      <A href=\"$1.html\">$1</A>\n",
                  "    </TD><TD valign=top>\n",
                  "      $headline $detailed\n",
                  "    </TD></TR>\n";
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
      $macro  = $1;
      $params = $2;
      $value  = $3;

      chomp $params;
      chomp $value;

      parse_macro( $macro, $params, $value );
   }
   elsif ( /^\s*\/\*\s*$/ ) {
      parse_comment( \$headline, \$detailed, \$options );
   }
   else {
      $headline = "";
      $detailed = "";
      %options  = ();
   }
}

print INDEX "  </TABLE></CENTER>\n",
            "</P>\n";

print_list( \%types, "Types" );
print_list( \%definitions, "Definitions" );


html_close( INDEX );
html_close( TYPES );





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





sub substitute_method_links ($)
   {
      my $str = shift(@_);

      # Interface Methods
      $$str =~ s/(I\w+)\:\:(\w+)\(\)/\<a\ href=\"\1\_\2\.html\"\>\1\:\:\2\(\)\<\/a\>/g;

      # Automatic type links
      $$str =~ s/(\s)([A-Z][A-Z][A-Z][A-Z]?[a-z][a-z][a-z]?[\w0-9]+)/\1\<a\ href=\"types\.html#\2\"\>\2\<\/a\>/g;

      # Explicit type links
      $$str =~ s/(\s)\@\_(\w[\w0-9]+)/\1\<a\ href=\"types\.html#\2\"\>\2\<\/a\>/g;
   }

sub parse_comment ($$$) {
   local (*head, *body, *options) = @_;
   my $headline_mode = 1;
   my $list_open = 0;

   $head = "";
   $body = "";

   %options = ();

   while (<>)
      {
         chomp;
         last if /^\s*\*\/\s*$/;

         if ($headline_mode == 1)
            {
               if (/^\s*\*\s*$/)
                  {
                     $headline_mode = 0;
                  }
               elsif (/^\s*\*\s*@(\w+)\s*=?\s*(.*)$/)
                  {
                     $options{$1} = $2;
                  }
               elsif (/^\s*\*\s*(.+)$/)
                  {
                     $head .= "  $1";
                  }
            }
         else
            {
               if (/^\s*\*\s*$/)
                  {
                     $body .= " </P><P>\n";
                  }
               elsif (/^\s*\*\s\-\s(.+)$/)
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

                     $body .= "  $1\n";
                  }
               elsif (/^\s*\*\s\s(.+)$/)
                  {
                     $body .= "  $1\n";
                  }
               elsif (/^\s*\*\s(.+)$/)
                  {
                     if ($list_open == 1)
                        {
                           $list_open = 0;

                           $body .= " </LI></UL>\n";
                        }

                     $body .= "  $1\n";
                  }
            }
      }

   if ($list_open == 1)
      {
         $body .= " </LI></UL>\n";
      }

   substitute_method_links (\$head);
   substitute_method_links (\$body);
}

sub parse_params () {
   my @entries;

   while (<>)
      {
         chomp;
         last if /^\s*\)\;\s*$/;

         if ( /^\s*(const )?\s*([\w\ ]+)\s+(\**)(\w+,?)\s*$/ )
            {
               my $const = $1;
               my $type  = $2;
               my $ptr   = $3;
               my $name  = $4;

               $type =~ s/\s*$//g;

               if ($types{$type})
                  {
                     $type = "$const<A href=\"types.html#$type\">$type</A>";
                  }
               elsif ($interface_abstracts{$type})
                  {
                     $type = "$const<A href=\"$type.html\">$type</A>";
                  }
               else
                  {
                     $type = "$const$type";
                  }

               my $rec = {
                  TYPE   => $type,
                  PTR    => $ptr,
                  NAME   => $name
               };

               push (@entries, $rec);
            }
      }

   return @entries;
}

#
# Reads stdin until the end of the interface is reached.
# Writes formatted HTML to one file for the interface and one file per method.
# Parameter is the interface name.
#
sub parse_interface (NAME)
   {
      my $interface = shift(@_);

      $section = "";

      html_create( INTERFACE, "$interface.html",
                              "<A href=\"index.html\">" .
                              "<FONT color=#DDDDDD>$PROJECT Interfaces</FONT>" .
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
                               "<FONT color=#DDDDDD>$interface</FONT>" .
                               "</A>", $2, "$interface - $2" );

                  print FUNCTION "<H4>$headline</H4>\n",
                                 "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=#F8F8F0>\n",
                                 "    <TR><TD colspan=5><I><FONT color=#425469><A href=\"types.html#$1\">$1</A> <b>$2 (</b></FONT></I></TD></TR>\n";

                  my @params = parse_params();

                  for my $param (@params)
                     {
                        print FUNCTION "    <TR><TD width=50>\n",
                                       "      &nbsp;\n",
                                       "    </TD><TD valign=top>\n",
                                       "      $param->{TYPE}\n",
                                       "    </TD><TD width=20>&nbsp;</TD><TD align=right>\n",
                                       "      <FONT color=#424242><B>$param->{PTR}</B></FONT>\n",
                                       "    </TD><TD valign=top>\n",
                                       "      <FONT color=#234269><B>$param->{NAME}</B></FONT>\n",
                                       "    </TD></TR>\n";
                     }

                  print FUNCTION "    <TR><TD colspan=5><I><FONT color=#425469><b>);</b></FONT></I></TD></TR>\n",
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
                  parse_comment( \$headline, \$detailed, \$options );
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
      %entries = ();
      @list = ();

      $pre = "";

      while (<>)
         {
            chomp;

            $entry = "";

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
                  $entries{ $entry } = $3;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\s*\*\/\s*$/ )
                           {
                              $entries{ $entry } .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $entries{ $entry } .= " $1";
                           }
                  }
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
                  $entries{ $entry } = $2;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\s*\*\/\s*$/ )
                           {
                              $entries{ $entry } .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $entries{ $entry } .= " $1";
                           }
                     }
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
                  $pre = $1;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\s*\*\/\s*$/ )
                           {
                              $pre .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $pre .= " $1";
                           }
                     }
               }
            # end of enum
            elsif ( /^\s*\}\s*(\w+)\s*\;\s*$/ )
               {
                  $enum = $1;

                  $types{$enum} = $headline;

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
                        "    <h3><font color=#B04223>$enum</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=#F8F8F0>\n";

            foreach $key (@list)
               {
                  substitute_method_links (\$entries{$key});

                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#429023><b>$key</b></font>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#234269>$values{$key}</font>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#424242>$entries{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "  </p><p>\n",
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
      @entries = ();
      %entries_params = ();
      %entries_types = ();
      %entries_ptrs = ();

      while (<>)
         {
            chomp;

            $entry = "";

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
                  $text = $6;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\*\/\s*$/ )
                           {
                              $text .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $text .= " $1";
                           }
                     }
               }
            elsif ( /^\s*\}\s*(\w+)\s*\;\s*$/ )
               {
                  $struct = $1;

                  $types{$struct} = $headline;

                  last;
               }

            if ($entry ne "")
               {
                  $type =~ s/\s*$//g;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "$const<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "$const<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$const$type";
                     }
   
                  $entries_ptrs{$entry} = $ptr;
                  $entries_params{$entry} = $text;

                  push (@entries, $entry);
               }
         }

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=\"$struct\" href=\"#$struct\">\n",
                        "    <h3><font color=#238423>$struct</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=#F8F8F0>\n";

            foreach $key (@entries)
               {
                  substitute_method_links (\$entries_params{$key});

                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top align=right>\n",
                              "      <FONT color=#424242>$entries_ptrs{$key}</FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=#234269><B>$key</B></FONT>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#424242>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "  </p><p>\n",
                        "  $detailed\n",
                        "</p><hr>\n";
         }
   }


sub parse_func (TYPE, NAME)
   {
      my $rtype = shift(@_);
      my $name  = shift(@_);

      @entries = ();
      %entries_params = ();
      %entries_types = ();
      %entries_ptrs = ();

      while (<>)
         {
            chomp;

            $entry = "";

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
                  $text = $6;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\*\/\s*$/ )
                           {
                              $text .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $text .= " $1";
                           }
                     }
               }
            elsif ( /^\s*\)\;\s*$/ )
               {
                  $types{$name} = $headline;

                  last;
               }

            if ($entry ne "")
               {
                  $type =~ s/\s*$//g;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "$const<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "$const<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$const$type";
                     }

                  $entries_ptrs{$entry} = $ptr;
                  $entries_params{$entry} = $text;

                  push (@entries, $entry);
               }
         }

      if ($types{$rtype})
         {
            $rtype = "<A href=\"types.html#$rtype\">$rtype</A>";
         }

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=\"$name\" href=\"#$name\">\n",
                        "    <h3><font color=#D06923>$name</font></h3>\n",
                        "  </a>\n",
                        "  <h4>$headline</h4>\n",
                        "  <TABLE border=0 cellspacing=4 cellpadding=2 bgcolor=#F8F8F0>\n",
                        "    <TR><TD colspan=4>\n",
                        "         <I><FONT color=#232342>$rtype (*$name) (</FONT></I>\n",
                        "    </TD></TR>\n";

            foreach $key (@entries)
               {
                  print TYPES "    <TR><TD width=32>\n",
                              "      &nbsp;\n",
                              "    </TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top align=right>\n",
                              "      <FONT color=#424242>$entries_ptrs{$key}</FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=#234269><B>$key</B></FONT>\n",
                              "    </TD><TD width=20>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#424242>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "    <TR><TD colspan=4><I><FONT color=#234269>);</FONT></I></TD></TR>\n",
                        "  </TABLE>\n",
                        "  </p><p>\n",
                        "  $detailed\n",
                        "</p><hr>\n";
         }
   }

#
# Reads stdin until the end of the macro is reached.
# Writes formatted HTML to "types.html".
# Parameter is the macro name.
#
sub parse_macro (NAME, PARAMS, VALUE)
   {
      my $macro  = shift(@_);
      my $params = shift(@_);
      my $value  = shift(@_);

      while (<>)
         {
            chomp;

            last unless /\\$/;
         }

      if (!defined ($options{"internal"}) && $value ne "") {
         $definitions{$macro} = $headline;

         $value =~ s/^\s*\\\s*$//;

         print TYPES "<p>\n",
                     "  <a name=\"$macro\" href=\"#$macro\">\n",
                     "    <h3><font color=#2342A0>$macro</font> <font color=#606080>$params</font></h3>\n",
                     "  </a>\n",
                     "  <h4>$headline</h4>\n",
                     "  <font color=#232342 size=+1><b>$value</b></font>\n",
                     "  </p><p>\n",
                     "  $detailed\n",
                     "</p><hr>\n";
      }
   }


sub html_create (FILEHANDLE, FILENAME, TITLE, SUBTITLE, SINGLETITLE)
   {
      my $FILE = shift(@_);
      my $filename = shift(@_);
      my $title = shift(@_);
      my $subtitle = shift(@_);
      my $singletitle = shift(@_);

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
                  "<BODY bgcolor=#FFF0D8 link=$COLOR_LINK vlink=$COLOR_LINK text=$COLOR_TEXT>\n",
                  "\n",
                  "<TABLE width=100% bgcolor=black border=0 cellspacing=1 cellpadding=3>\n",
                  "  <TR><TD width=30%>\n",
                  "    <A href=\"http://www.directfb.org\"><IMG border=0 src=\"directfb.png\"></A>\n",
                  "  </TD><TD align=right>\n",
                  "    &nbsp;&nbsp;",
                  "    <A href=\"index.html\"><FONT size=+3 color=white>Reference Manual - $VERSION</FONT></A>\n",
                  "  </TD></TR>\n",
                  "  <TR><TD colspan=2 align=center bgcolor=#203040>\n";

      if ($subtitle)
         {
            print $FILE "    <TABLE border=0 cellspacing=0 cellpadding=0>\n",
                        "      <TR><TD nowrap align=right width=50%>\n",
                        "        <BIG>$title&nbsp;</BIG>\n",
                        "      </TD><TD nowrap align=left width=50%>\n",
                        "        <BIG><FONT color=orange>&nbsp;$subtitle</FONT></BIG>\n",
                        "      </TD></TR>\n",
                        "    </TABLE>\n";
         }
      else
         {
            print $FILE "    <BIG><FONT color=orange>$title</FONT></BIG>\n";
         }

      print $FILE "  </TD></TR>\n",
                  "</TABLE>\n",
                  "\n";
   }

sub html_close (FILEHANDLE)
   {
      my $FILE = shift(@_);

      print $FILE "\n",
                  "<TABLE width=100% bgcolor=#e0e8f0 border=0 cellspacing=1 cellpadding=3>\n",
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

