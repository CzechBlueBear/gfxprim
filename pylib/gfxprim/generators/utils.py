# Module with templates and tools for writing generated code, 
# especially C source and headers
#
# 2011 - Tomas Gavenciak <gavento@ucw.cz> 
#

import sys, os, time
import jinja2 
from gfxprim.generators.generator import known_generators

def load_generators():
  "Load all modules containig generators to allow them to register"
  # TODO: write
  pass

def generate_file(fname):
  "Function trying to generate file `fname` using matching known generator."
  matches = []
  for k in known_generators:
    if k.matches_path(fname):
      matches.append(k)
  if len(matches) == 0:
    die("No known generators match '%s'" % (fname,))
  if len(matches) > 1:
    die("Multiple generators match '%s': %s" % (fname, str(matches)))
  s = matches[0].generate()
  with open(fname, "wt") as f:
    f.write(s)

def j2render(tmpl, **kw):
  "Internal helper to render jinja2 templates (with StrictUndefined)"
  t2 = tmpl.rstrip('\n') # Jinja strips the last '\n', so add these later
  return jinja2.Template(t2, undefined=jinja2.StrictUndefined).render(**kw) + tmpl[len(t2):]

