import Foundation
let samples: [(String, String, Bool, Bool)] = [
    ("hello", "HELLO", false, true),
    ("тест", "ТЕСТ", false, true),
    ("广告", "这是广告", false, true),
    ("رسالة", "هذه رسالة", false, true),
    ("👩‍💻", "你好👩‍💻", false, true),
    ("a.b", "axb", false, false),
    ("café", "CAFE\u{301}", false, true),
    ("strasse", "Straße", false, true),
    ("[a-z&&[^aeiou]]+", "BCD", true, true),
    ("[a-z&&[^aeiou]]+", "aeiou", true, false),
    ("(?<=prefix )value", "PREFIX VALUE", true, true),
    ("\\p{L}+", "123中文", true, true)
]
for (pattern, text, regex, expected) in samples {
    let matched: Bool
    if regex {
        let expression = try NSRegularExpression(pattern: pattern, options: [.caseInsensitive])
        matched = expression.firstMatch(in: text, range: NSRange(location: 0, length: (text as NSString).length)) != nil
    } else {
        matched = text.range(of: pattern, options: [.caseInsensitive]) != nil
    }
    if matched != expected {
        fatalError("Unexpected Foundation result: \(pattern)")
    }
    print("PASS", regex ? "regex" : "literal", pattern)
}
