"""Tests for tools/build_web.py: python -m unittest discover -s tools -p "test_*.py" """
import importlib.util
import os
import re
import shutil
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("build_web", os.path.join(HERE, "build_web.py"))
build_web = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_web)


def write(folder, name, content):
    path = os.path.join(folder, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


class InlinePageTest(unittest.TestCase):
    def test_inlines_stylesheet_and_scripts_in_order(self):
        with tempfile.TemporaryDirectory() as folder:
            write(folder, "index.html",
                  '<link rel="stylesheet" href="app.css">'
                  '<script src="js/a.js"></script><script src="js/b.js"></script>')
            write(folder, "app.css", "body{color:red}")
            write(folder, "js/a.js", "let a = 1;")
            write(folder, "js/b.js", "let b = 2;")

            page = build_web.inline_page(folder)

            self.assertEqual(page,
                             "<style>\nbody{color:red}\n</style>"
                             "<script>\nlet a = 1;\n</script><script>\nlet b = 2;\n</script>")

    def test_missing_file_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            write(folder, "index.html", '<script src="js/missing.js"></script>')
            with self.assertRaises(FileNotFoundError):
                build_web.inline_page(folder)


class StripTest(unittest.TestCase):
    def test_js_loses_comment_lines_indentation_and_empty_lines(self):
        source = "// header\nconst A = (() => {\n\n  // why\n  let x = 1;   // keeps an end comment\n  return x;\n})();\n"
        self.assertEqual(build_web.strip_js(source),
                         "const A = (() => {\nlet x = 1;   // keeps an end comment\nreturn x;\n})();")

    def test_css_loses_block_comments(self):
        source = "/* Cards */\n.card {\n  color: red; /* why */\n}\n/* two\n   lines */\n.b { }\n"
        self.assertEqual(build_web.strip_css(source), ".card {\ncolor: red;\n}\n.b { }")

    @unittest.skipUnless(shutil.which("node"), "needs node")
    def test_every_script_of_the_real_page_still_parses(self):
        page = build_web.inline_page(os.path.join(os.path.dirname(HERE), "web"))
        scripts = re.findall(r"<script>\n(.*?)\n</script>", page, flags=re.S)
        self.assertGreater(len(scripts), 10)
        with tempfile.TemporaryDirectory() as folder:
            for index, script in enumerate(scripts):
                path = os.path.join(folder, "script%d.js" % index)
                with open(path, "w", encoding="utf-8") as f:
                    f.write(script)
                result = subprocess.run(["node", "--check", path], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)


class HeaderTest(unittest.TestCase):
    def test_writes_bytes_length_and_quoted_etag(self):
        text = build_web.header(bytes([0x1f, 0x8b, 0x00]), "abc123", bytes([0x89, 0x50]))

        self.assertIn('const char WEB_APP_ETAG[] = "\\"abc123\\"";', text)
        self.assertIn("const size_t WEB_APP_GZ_LEN = 3;", text)
        self.assertIn("0x1f, 0x8b, 0x00,", text)
        self.assertIn("PROGMEM", text)

    def test_writes_touch_icon(self):
        text = build_web.header(bytes([0x1f]), "abc123", bytes([0x89, 0x50, 0x4e]))

        self.assertIn("const size_t WEB_TOUCH_ICON_LEN = 3;", text)
        self.assertIn("const uint8_t WEB_TOUCH_ICON[] PROGMEM = {\n    0x89, 0x50, 0x4e,\n};", text)


if __name__ == "__main__":
    unittest.main()
