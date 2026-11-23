// A tiny stand-in for the browser DOM, just enough for widgets that build their own elements
// (StepRows). Focus is tracked in document.activeElement; events are dispatched by hand.
class FakeNode {
  constructor(tagName) {
    this.tagName = tagName.toUpperCase();
    this.children = [];
    this.parentNode = null;
    this.attributes = {};
    this.listeners = {};
    this.className = "";
    this.hidden = false;
    this.value = "";
    this.id = "";
    this._text = "";
    this.innerHTML = "";
  }

  get classList() {
    const node = this;
    const names = () => node.className.split(/\s+/).filter(Boolean);
    return {
      contains: (name) => names().includes(name),
      add: (name) => { if (!names().includes(name)) node.className = names().concat(name).join(" "); },
      remove: (name) => { node.className = names().filter((n) => n !== name).join(" "); },
      toggle: (name, on) => {
        const want = on === undefined ? !names().includes(name) : Boolean(on);
        if (want) node.classList.add(name); else node.classList.remove(name);
        return want;
      },
    };
  }

  get textContent() { return this._text + this.children.map((c) => c.textContent).join(""); }
  set textContent(text) { this.children.forEach((c) => { c.parentNode = null; }); this.children = []; this._text = String(text); }

  setAttribute(name, value) { this.attributes[name] = String(value); if (name === "id") this.id = String(value); }
  getAttribute(name) { return name in this.attributes ? this.attributes[name] : null; }

  addEventListener(type, fn) { (this.listeners[type] = this.listeners[type] || []).push(fn); }
  dispatch(type) { (this.listeners[type] || []).forEach((fn) => fn({ type, target: this })); }

  append(...nodes) { nodes.forEach((n) => this.insertBefore(typeof n === "string" ? textNode(n) : n, null)); }
  appendChild(node) { return this.insertBefore(node, null); }
  insertBefore(node, before) {
    if (node.parentNode) node.remove();
    const index = before ? this.children.indexOf(before) : -1;
    if (index < 0) this.children.push(node); else this.children.splice(index, 0, node);
    node.parentNode = this;
    return node;
  }
  remove() {
    if (!this.parentNode) return;
    if (this.contains(document.activeElement)) document.activeElement = document.body;
    this.parentNode.children = this.parentNode.children.filter((c) => c !== this);
    this.parentNode = null;
  }
  replaceChildren(...nodes) { [...this.children].forEach((c) => c.remove()); this.append(...nodes); }
  contains(node) {
    for (let n = node; n; n = n.parentNode) if (n === this) return true;
    return false;
  }
  focus() { document.activeElement = this; }
  blur() { if (document.activeElement === this) document.activeElement = document.body; }

  // Only ".class" and "tag" selectors
  querySelectorAll(selector) {
    const match = selector.startsWith(".")
      ? (n) => n.classList.contains(selector.slice(1))
      : (n) => n.tagName === selector.toUpperCase();
    const found = [];
    const walk = (n) => n.children.forEach((c) => { if (match(c)) found.push(c); walk(c); });
    walk(this);
    return found;
  }
  querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
}

function textNode(text) {
  const node = new FakeNode("#text");
  node._text = text;
  return node;
}

const document = {
  body: new FakeNode("body"),
  activeElement: null,
  createElement: (tag) => new FakeNode(tag),
  getElementById(id) {
    const walk = (n) => {
      for (const c of n.children) {
        if (c.id === id) return c;
        const found = walk(c);
        if (found) return found;
      }
      return null;
    };
    return walk(document.body);
  },
};
document.activeElement = document.body;

function install() {
  global.document = document;
  global.Option = function Option(text) { const o = new FakeNode("option"); o.value = text; o._text = text; return o; };
  document.body.replaceChildren();
  document.activeElement = document.body;
}

module.exports = { install, document, FakeNode };
