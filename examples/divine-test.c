#include <unistd.h>
#include <string.h>

#include <divine.h>

int main (int argc, char *argv[])
{
  int         i;
  DiVine     *divine;
  const char *msg = "DiVine Test!";
  int         len = strlen (msg);

  /* open the connection to the input driver */
  divine = divine_open ("/tmp/divine");
  if (!divine)
    return -1;

  /* wait a bit */
  sleep (3);

  /* write a string */
  for (i=0; i<len; i++)
    {
      /* simulate typing each character */
      divine_send_symbol (divine, msg[i]);

      /* wait a second */
      sleep (1);
    }

  /* simulate a press/release of escape */
  divine_send_symbol (divine, DIKS_ESCAPE);

  /* close the connection */
  divine_close (divine);

  return 0;
}
