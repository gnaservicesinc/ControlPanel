// Rebuild the checked-in icons with: ./scripts/make-icons.sh
import AppKit

let destination = CommandLine.arguments[1]
for app in ["Interpreter", "Creator"] {
    let folder = URL(fileURLWithPath: destination).appendingPathComponent("\(app).iconset")
    try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
    for points in [16, 32, 128, 256, 512] {
        for scale in [1, 2] {
            let size = points * scale
            let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
                bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
            NSGraphicsContext.saveGraphicsState()
            NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
            let context = NSGraphicsContext.current!.cgContext
            context.scaleBy(x: CGFloat(size) / 1024, y: CGFloat(size) / 1024)
            let background = NSBezierPath(roundedRect: NSRect(x: 70, y: 70, width: 884, height: 884), xRadius: 196, yRadius: 196)
            NSGradient(starting: NSColor(calibratedRed: 0.06, green: 0.19, blue: 0.28, alpha: 1),
                       ending: NSColor(calibratedRed: 0.10, green: 0.37, blue: 0.43, alpha: 1))!.draw(in: background, angle: 65)
            NSColor.white.withAlphaComponent(0.85).setFill()
            NSBezierPath(roundedRect: NSRect(x: 212, y: 716, width: 410, height: 38), xRadius: 19, yRadius: 19).fill()
            for row in 0..<2 {
                for column in 0..<3 {
                    let highlight = row == 0 && column == 2
                    (highlight ? NSColor(calibratedRed: 0.35, green: 0.86, blue: 0.73, alpha: 1) : NSColor.white.withAlphaComponent(0.9)).setFill()
                    NSBezierPath(roundedRect: NSRect(x: 212 + column * 210, y: 272 + row * 192, width: 174, height: 144), xRadius: 28, yRadius: 28).fill()
                }
            }
            let badge = NSBezierPath(ovalIn: NSRect(x: 650, y: 90, width: 280, height: 280))
            NSColor(calibratedRed: 1, green: 0.74, blue: 0.30, alpha: 1).setFill(); badge.fill()
            NSColor(calibratedRed: 0.08, green: 0.22, blue: 0.29, alpha: 1).setFill()
            if app == "Creator" {
                NSBezierPath(roundedRect: NSRect(x: 714, y: 213, width: 152, height: 34), xRadius: 12, yRadius: 12).fill()
                NSBezierPath(roundedRect: NSRect(x: 773, y: 154, width: 34, height: 152), xRadius: 12, yRadius: 12).fill()
            } else {
                let play = NSBezierPath(); play.move(to: NSPoint(x: 750, y: 153)); play.line(to: NSPoint(x: 858, y: 230));
                play.line(to: NSPoint(x: 750, y: 307)); play.close(); play.fill()
            }
            NSGraphicsContext.restoreGraphicsState()
            let suffix = scale == 2 ? "@2x" : ""
            try bitmap.representation(using: .png, properties: [:])!.write(to: folder.appendingPathComponent("icon_\(points)x\(points)\(suffix).png"))
        }
    }
}
