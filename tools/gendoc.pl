#!/usr/bin/perl
#
#   (c) Copyright 2000-2002  convergence integrated media GmbH.
#   (c) Copyright 2002       convergence GmbH.
#
#   All rights reserved.
#
#   Written by Denis Oliver Kropp <dok@directfb.org>,
#              Andreas Hundt <andi@fischlustig.de> and
#              Sven Neumann <sven@convergence.de>.
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

################################################################
#                                                              #
#  Documentation generator (early stage of implementation)     #
#                                                              #
#  - Reads header from stdin                                   #
#  - Writes HTML to several files                              #
#                                                              #
################################################################


html_create( INDEX, "index.html", "Index Page" );
html_create( TYPES, "types.html", "DirectFB Types" );

print INDEX "<P>\n",
            "  <CENTER>\n",
            "    <H3>Interfaces</H3>\n",
            "    <TABLE width=90% border=0 cellpadding=2>\n";

while (<>) {
   chomp;

   if ( /^\s*\/\*\s*$/ ) {
      %options = ();
      $comment = "";

      while (<>) {
         chomp;

         last if ( /^\s*\*\/\s*$/ );

         if (/^\s*\*\s*(.*)$/) {
            $line = $1;

            if ($line eq "" && $comment ne "") {
               $comment .= "    <br><br>\n";
            }
            elsif ($line =~ /^@(\w+)\s*=?\s*(.*)$/) {
               $options{$1} = $2;
            }
            else {
               $comment .= "      $line\n";
            }
         }
      }

      substitute_method_links (\$comment);
   }
   elsif ( /^\s*DECLARE_INTERFACE\s*\(\s*(\w+)\s\)\s*$/ ) {
      $interface_abstracts{$1} = $comment;

      print INDEX "    <TR><TD valign=top>\n",
                  "      <A href=\"$1.html\">$1</A>\n",
                  "    </TD><TD valign=top>\n",
                  "      $comment\n",
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
   elsif ( /^\s*#define\s+([^\(\s]+)\s*(\([^\)]*\))?/ ) {
      $macro  = $1;
      $params = $2;

      chomp $params;

      if ($comment ne "" && !defined ($options{"internal"})) {
         parse_macro( $macro, $params );
      }
   }
   else {
      $comment = "";
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

      $$str =~ s/(I\w+)\:\:(\w+)\(\)/\<a\ href=\"\1\_\2\.html\"\>\1\:\:\2\(\)\<\/a\>/g;
   }

sub parse_comment ($$) {
   local (*head, *body) = @_;
   my $headline_mode = 1;

   $head = "";
   $body = "";


   while (<>)
      {
         chomp;
         last if /^\s*\*\/\s*$/;

         if ($headline_mode == 1)
            {
               if (/^\s*\*?\s*$/)
                  {
                     $headline_mode = 0;
                  }
               elsif (/^\s*\*?\s*(.+)$/)
                  {
                     $head .= " $1";
                  }
            }
         else
            {
               if (/^\s*\*?\s*$/)
                  {
                     $body .= " </P><P>";
                  }
               elsif (/^\s*\*?\s*(.+)$/)
                  {
                     $body .= " $1";
                  }
            }
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

      $headline = "";
      $detailed = "";
      $section = "";

      html_create( INTERFACE, "$interface.html",
                              "<A href=\"index.html\">" .
                              "<FONT color=#DDDDDD>DirectFB Interfaces</FONT>" .
                              "</A>", $interface );

#      print INTERFACE "<P align=center>\n",
#                      "  $interface_abstracts{$interface}\n",
#                      "</P>";

      print INTERFACE "<p style=\"margin\-left:3%; margin\-right:3%;\"\>$comment</p>";

      print INTERFACE "<P>\n",
                      "  <CENTER><TABLE width=93% border=1 rules=groups cellpadding=2 cellspacing=0>\n";

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
            elsif ( /^\s*DFBResult\s*\(\s*\*\s*(\w+)\s*\)\s*\(?\s*$/ )
               {
                  print INTERFACE "    <TR><TD valign=top>\n",
                                  "      <B><SMALL>$section</SMALL></B>\n",
                                  "    </TD><TD valign=top>\n",
                                  "      <A href=\"${interface}_$1.html\">",
                                  "      <B>$1</B></A>\n",
                                  "    </TD><TD valign=top>\n",
                                  "      $headline\n",
                                  "    </TD></TR>\n";

                  html_create( FUNCTION, "${interface}_$1.html",
                               "<A href=\"$interface.html\">" .
                               "<FONT color=#DDDDDD>$interface</FONT>" .
                               "</A>", $1 );

                  print FUNCTION "<P>$headline</P><P>\n",
                                 "  <TABLE border=0 cellspacing=0 cellpadding=2 bgcolor=#C0C0C0>\n",
                                 "    <TR><TD colspan=5><I><FONT color=black>$1 (</FONT></I></TD></TR>\n";

                  my @params = parse_params();

                  for my $param (@params)
                     {
                        print FUNCTION "    <TR><TD width=50>&nbsp;</TD><TD valign=top>\n",
                                       "      $param->{TYPE}\n",
                                       "    </TD><TD width=20>&nbsp;</TD><TD align=right>\n",
                                       "      <FONT color=black><B>$param->{PTR}</B></FONT>\n",
                                       "    </TD><TD valign=top>\n",
                                       "      <FONT color=black><B>$param->{NAME}</B></FONT>\n",
                                       "    </TD></TR>\n";
                     }

                  print FUNCTION "    <TR><TD colspan=3><I><FONT color=black>);</FONT></I></TD></TR>\n",
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
                  parse_comment( \$headline, \$detailed );
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
            if ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\!\s]+)\s*,?\s*\/\*\s*(.+)\s*\*\/\s*$/ )
               {
                  $entry = $1;
                  $entries{ $entry } = $3;
               }
            # entry with assignment (opening comment)
            elsif ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\!\s]+)\s*,?\s*\/\*\s*(.+)\s*$/ )
               {
                  $entry = $1;

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
            elsif ( /^\s*(\w+)\s*=\s*([\w\d\(\)\,\!\s]+)\s*,?\s*$/ )
               {
                  $entry = $1;
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

                  $types{$enum} = $comment;

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
                        "  <a name=$enum>\n",
                        "  <font color=#D07070 size=+1>$enum</font>\n";

            print TYPES "  <br>\n",
                        "  <TABLE border=0 cellspacing=0 cellpadding=4 bgcolor=#F0F0F0>\n";

            foreach $key (@list)
               {
                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      <font color=#40A040>$key</font>\n",
                              "    </TD><TD valign=top>\n",
                              "      <font color=#404040>$entries{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "  $comment\n",
                        "  <br>\n",
                        "</p>\n";
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

      while (<>)
         {
            chomp;

            # without comment
            if ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+;)\s*$/ )
               {
                  $type = $1;
                  $entry = $2;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = "";
               }
            # complete one line entry
            elsif ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+;)\s*\/\*\s*(.+)\*\/\s*$/ )
               {
                  $type = $1;
                  $entry = $2;
                  $text = $3;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = $text;
               }
            # with comment opening
            elsif ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+;)\s*\/\*\s*(.+)\s*$/ )
               {
                  $type = $1;
                  $entry = $2;
                  $text = $3;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = $text;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\*\/\s*$/ )
                           {
                              $entries_params{ $entry } .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $entries_params{ $entry } .= " $1";
                           }
                     }
               }
            elsif ( /^\s*\}\s*(\w+)\s*\;\s*$/ )
               {
                  $struct = $1;

                  $types{$struct} = $comment;

                  last;
               }
         }

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=$struct>",
                        "  <font color=#70D070 size=+1>$struct</font>\n";

            print TYPES "  <br>\n",
                        "  <TABLE border=0 cellspacing=0 cellpadding=4 bgcolor=#E0E0E0>\n";

            foreach $key (@entries)
               {
                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=black><B>$key</B></FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <font color=#404040>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "  </TABLE>\n",
                        "  $comment\n",
                        "  <br>\n",
                        "</p>\n";

         }
   }


sub parse_func (TYPE, NAME)
   {
      my $rtype = shift(@_);
      my $name  = shift(@_);

      @entries = ();
      %entries_params = ();
      %entries_types = ();

      while (<>)
         {
            chomp;

            # without comment
            if ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+,?)\s*$/ )
               {
                  $type = $1;
                  $entry = $2;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = "";
               }
            # complete one line entry
            elsif ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+,?)\s*\/\*\s*(.+)\*\/\s*$/ )
               {
                  $type = $1;
                  $entry = $2;
                  $text = $3;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = $text;
               }
            # with comment opening
            elsif ( /^\s*([\w\ ]+)\s+(\**[\w\d\+\[\]]+,?)\s*\/\*\s*(.+)\s*$/ )
               {
                  $type = $1;
                  $entry = $2;
                  $text = $3;

                  $type =~ s/\ *$//;

                  if ($types{$type})
                     {
                        $entries_types{$entry} = "<A href=\"types.html#$type\">$type</A>";
                     }
                  elsif ($interface_abstracts{$type})
                     {
                        $entries_types{$entry} = "<A href=\"$type.html\">$type</A>";
                     }
                  else
                     {
                        $entries_types{$entry} = "$type";
                     }

                  push (@entries, $entry);
                  $entries_params{ $entry } = $text;

                  while (<>)
                     {
                        chomp;

                        if ( /^\s*(.+)\*\/\s*$/ )
                           {
                              $entries_params{ $entry } .= " $1";
                              last;
                           }
                        elsif ( /^\s*(.+)\s*$/ )
                           {
                              $entries_params{ $entry } .= " $1";
                           }
                     }
               }
            elsif ( /^\s*\)\;\s*$/ )
               {
                  $types{$name} = $comment;

                  last;
               }
         }

      if ($types{$rtype})
         {
            $rtype = "<A href=\"types.html#$rtype\">$rtype</A>";
         }

      if (scalar @entries > 0)
         {
            print TYPES "<p>",
                        "  <a name=$name>",
                        "  <font color=#40A0F0 size=+1>$name</font>\n",
                        "  <br>\n",
                        "  <TABLE border=0 cellspacing=0 cellpadding=4 bgcolor=#E0E0E0>\n",
                        "    <TR><TD colspan=4>\n",
                        "         <I><FONT color=black>$rtype (*$name) (</FONT></I>\n",
                        "    </TD></TR>\n";

            foreach $key (@entries)
               {
                  print TYPES "    <TR><TD width=32>&nbsp;</TD><TD valign=top>\n",
                              "      $entries_types{$key}\n",
                              "    </TD><TD valign=top>\n",
                              "      <FONT color=black><B>$key</B></FONT>\n",
                              "    </TD><TD valign=top>\n",
                              "      <font color=#404040>$entries_params{$key}</font>\n",
                              "    </TD></TR>\n";
               }

            print TYPES "    <TR><TD colspan=3><I><FONT color=black>);</FONT></I></TD></TR>\n",
                        "  </TABLE>\n",
                        "  $comment\n",
                        "</p>\n";

         }
   }

#
# Reads stdin until the end of the macro is reached.
# Writes formatted HTML to "types.html".
# Parameter is the macro name.
#
sub parse_macro (NAME, PARAMS)
   {
      my $macro  = shift(@_);
      my $params = shift(@_);

      while (<>)
         {
            chomp;

            last unless /\\$/;
         }

      $definitions{"$macro $params"} = $comment;

      print TYPES "<p>\n",
                  "  <a name=\"$macro $params\">\n",
                  "  <font color=#7070D0 size=+1>$macro $params</font>\n",
                  "  <br>\n",
                  "  $comment\n",
                  "  <br>\n",
                  "</p>\n";
   }


sub html_create (FILEHANDLE, FILENAME, TITLE, SUBTITLE)
   {
      my $FILE = shift(@_);
      my $filename = shift(@_);
      my $title = shift(@_);
      my $subtitle = shift(@_);

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
                  "  <TITLE>DirectFB Reference Manual</TITLE>\n",
                  "</HEAD>\n",
                  "<BODY bgcolor=#FFFFFF link=#0070FF",
                       " vlink=#0070FF text=#404040>\n",
                  "\n",
                  "<TABLE width=100% bgcolor=black border=0 cellspacing=1 cellpadding=3>\n",
                  "  <TR><TD width=30%>\n",
                  "    <A href=\"http://www.directfb.org\"><IMG border=0 src=\"directfb.png\"></A>\n",
                  "  </TD><TD align=right>\n",
                  "    &nbsp;&nbsp;",
                  "    <A href=\"index.html\"><FONT size=+3 color=white>DirectFB Reference Manual</FONT></A>\n",
                  "  </TD></TR>\n",
                  "  <TR><TD colspan=2 align=center bgcolor=#303030>\n";

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
                  "<TABLE width=100% bgcolor=black border=0 cellspacing=0 cellpadding=0>\n",
                  "  <TR><TD valign=center>\n",
                  "    <FONT size=-3>&nbsp;&nbsp;(C) Copyright by convergence GmbH</FONT>\n",
                  "  </TD><TD align=right>\n",
                  "    <A href=\"http://www.convergence.de\">\n",
                  "      <IMG border=0 hspace=4 src=\"cimlogo.png\">\n",
                  "    </A>\n",
                  "  </TD></TR>\n",
                  "</TABLE>\n",
                  "\n",
                  "</BODY>\n",
                  "</HTML>\n";

      close( $FILE );
   }

