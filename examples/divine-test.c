#include <unistd.h>

#include <divine.h>

int main (int argc, char *argv[])
{
  int     i;
  DiVine *divine;

  /* open the connection to the input driver */
  divine = divine_open ("/tmp/divine");
  if (!divine)
    return -1;

  /* wait a bit */
  sleep (3);

  /* write a string */
  for (i=0; i<12; i++)
    {
      /* simulate typing each character */
      divine_send_symbol (divine, "DiVine Test!"[i]);

      /* wait a second */
      sleep (1);
    }

  /* simulate a press/release of escape */
  divine_send_symbol (divine, DIKS_ESCAPE);

  /* close the connection */
  divine_close (divine);

  return 0;
}
