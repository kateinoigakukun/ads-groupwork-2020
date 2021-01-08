import Foundation

extension String {
    func split(by length: Int) -> [String] {
        var startIndex = self.startIndex
        var results = [Substring]()

        while startIndex < self.endIndex {
            let endIndex = self.index(startIndex, offsetBy: length, limitedBy: self.endIndex) ?? self.endIndex
            results.append(self[startIndex..<endIndex])
            startIndex = endIndex
        }

        return results.map { String($0) }
    }
}

for file in CommandLine.arguments[1...] {
    let contents = try String(
        contentsOf: URL(fileURLWithPath: file), encoding: .ascii
    )
    let result = contents.split(by: 50).joined(separator: "\n")
    try result.write(toFile: file, atomically: true, encoding: .utf8)
}
