
import os, random, string

try:
    from tqdm import tqdm
except:
    class tqdm:
        def __init__(*args, **kwargs): pass
        def update(*args): pass
        def close(*args): pass

# In order to guarantee identical generation between Python2 and Python3 here I implement these myself.
# They changed the implementation of the integer selecting functions, but not random.random.
random_float = random.random

def randrange(x):
    return int(random.random() * x)

def choice(l):
    return l[randrange(len(l))]

keywords = {"let", "fun"}

identifier_starting_letters = string.ascii_letters + "_"
identifier_letters = identifier_starting_letters + string.digits

def generate_name():
    while True:
        name = choice(identifier_starting_letters) + "".join(
            choice(identifier_letters)
            for _ in range(randrange(6))
        )
        if name not in keywords:
            return name

def generate_string_literal():
    # Make sure the competitor doesn't mess up on keywords in strings.
    if random_float() < 0.001:
        # For Python2 + Python3 exact equality we have to sort here.
        return choice(['"%s"' % kw for kw in sorted(keywords)])
    choices = list(string.ascii_lowercase + string.punctuation + " ")
    choices.remove('"')
    choices.remove("\\")
    # Various escape sequences.
    choices += ["\\\\", "\\n", "\\\""]
    # Try to trigger bugs in handling of comments.
    choices += ["/*", "*/", "//"]
    return '"%s"' % "".join(choice(choices) for _ in range(randrange(40)))

def space():
    return " "[:random_float() < 0.1]

def likely_space():
    return " "[:random_float() < 0.9]

def generate_expression(depth=0):
    if depth < 2:
        which = choice(["atom", "fun", "fun", "app", "app", "app", "parens"])
    else:
        # In order to have finite expectation we need the expected recursion to be less than one.
        # Here we recurse [0, 0, 1, 2, 1] times respectively, for an average recursion of 0.8.
        which = choice(["atom", "atom", "fun", "app", "parens"])

    if which == "atom":
        if random_float() < 0.1:
            return generate_string_literal()
        else:
            return generate_name()
    if which == "fun":
        return "fun %s%s%s=>%s%s" % (space(), generate_name(), space(), space(), generate_expression(depth + 1))
    if which == "app":
        return "%s %s%s" % (generate_expression(depth + 1), space(), generate_expression(depth + 1))
    if which == "parens":
        return "(%s%s%s)" % (space(), generate_expression(depth + 1), space())
    assert False

def generate_comment_line_contents(block_safe):
    # Make sure the competitor appropriately handles code formatted in comments.
    if random_float() < 0.25:
        return generate_simple_line(block_safe=block_safe).strip()
    # Otherwise return gibberish.
    return " ".join(generate_name() for _ in range(3 + randrange(6)))

def make_block_safe(s):
    # Because code is free to include /* and */ in string literals in unbalanced ways we need to clean that out here.
    while "/*" in s or "*/" in s:
        s = s.replace("/*", "").replace("*/", "")
    return s

def generate_simple_line(block_safe):
    if random_float() < 0.1:
        if random_float() < 0.5:
            return "// %s\n" % generate_comment_line_contents(block_safe=block_safe)
        else:
            return "/* %s */\n" % generate_comment_line_contents(block_safe=True)
    s = "let %s%s%s:=%s%s;\n" % (space(), generate_name(), likely_space(), likely_space(), generate_expression())
    if block_safe:
        s = make_block_safe(s)
    return s

def generate_lines(line_count):
    indentation_level = 0
    source_lines = 0
    lines = []

    progress_bar = tqdm(total=line_count)
    while source_lines < line_count:
        indentation = " " * (4*indentation_level)

        which = choice(12*["simple"] + 3*["blank"] + ["block_comment"])
        if which == "simple":
            l = generate_simple_line(block_safe=False) 
            lines.append(indentation + l)
            if l.startswith("let"):
                source_lines += 1
                progress_bar.update(1)
        elif which == "block_comment":
            lines.append(indentation + "/*\n")
            for _ in range(choice([1, 1, 2, 3])):
                lines.append(indentation + generate_comment_line_contents(block_safe=True) + "\n")
            lines.append(indentation + "*/\n")
        elif which == "blank":
            lines.append("\n")
        else:
            assert False

        indentation_level = max(0, indentation_level + choice(3*[0] + 3*[+1] + 4*[-1]))
    progress_bar.close()

    return lines

if __name__ == "__main__":
    for name, length in [
        (  "1k",    1),
        ( "10k",   10),
        ("100k",  100),
        (  "1M", 1000),
    ]:
        # Fix the seed to deterministically generate the same test files every time.
        random.seed(12345 + length)

        path = os.path.join("files", "source_%s.txt" % name)
        print("Filling:", path)
        lines = generate_lines(1000 * length)
        with open(path, "w") as f:
            f.write("".join(lines))

