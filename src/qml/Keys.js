.pragma library

// Key names as the reader's keyboard labels them ("Strg" for Ctrl in German),
// through the translation catalog. Letters and symbols stay as they are.
function label(text) {
    return String(text).replace(/\b(Ctrl|Shift|Esc|PgUp|PgDn|Space|Backspace|Enter|Delete|Arrows)\b/g,
                                (name) => qsTranslate("Keys", name))
}
