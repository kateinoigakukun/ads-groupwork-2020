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

let segmentBodySize = 9

func loadSegments(_ file: String) -> [Int] {
    var contents = try! String(
        contentsOf: URL(fileURLWithPath: file)
    )
    contents = String(contents[..<contents.index(before: contents.endIndex)])
    return contents.split(by: segmentBodySize).map {
        Int($0, radix: 2)!
    }
}

let lhs = loadSegments(CommandLine.arguments[1])
let rhs = loadSegments(CommandLine.arguments[2])

func hummingDistance(_ lhs: Int, _ rhs: Int) -> Int {
    var xored = lhs ^ rhs
    var distance = 0
    while (xored != 0) {
        xored &= xored - 1
        distance += 1
    }
    return distance
}

var stats = Array(repeating: 0, count: segmentBodySize + 1)

for (lLine, rLine) in zip(lhs, rhs) {
    let distance = hummingDistance(lLine, rLine)
    stats[distance] += 1
}

let totalDistance = stats.enumerated().reduce(0) {
    $0 + $1.element * $1.offset
}

let totalSegments = Double(200000)/Double(segmentBodySize)
print("total: \(totalDistance)")

print("| diff | segment count | total per | diff bit count |")
for (index, count) in stats.enumerated() {
    let totalPer = Double(count)/totalSegments * 100
    print("| \(String(format: "%4d", index)) | \(String(format: "%13d", count)) | \(String(format: "%7.3f", totalPer)) % | \(String(format: "%14d", count * index)) |")
}

