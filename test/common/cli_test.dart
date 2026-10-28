import 'package:fl_clash/common/cli.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('CliOptions default values', () {
    const options = CliOptions();
    expect(options.isMinimized, isFalse);
    expect(options.isStart, isFalse);
  });

  test('CliOptions parses empty arguments', () {
    final options = CliOptions.fromArgs([]);
    expect(options.isMinimized, isFalse);
    expect(options.isStart, isFalse);
  });

  test('CliOptions parses --minimized', () {
    final options = CliOptions.fromArgs(['--minimized']);
    expect(options.isMinimized, isTrue);
    expect(options.isStart, isFalse);
  });

  test('CliOptions parses -m alias', () {
    final options = CliOptions.fromArgs(['-m']);
    expect(options.isMinimized, isTrue);
    expect(options.isStart, isFalse);
  });

  test('CliOptions parses --hidden alias', () {
    final options = CliOptions.fromArgs(['--hidden']);
    expect(options.isMinimized, isTrue);
    expect(options.isStart, isFalse);
  });

  test('CliOptions parses --start', () {
    final options = CliOptions.fromArgs(['--start']);
    expect(options.isMinimized, isFalse);
    expect(options.isStart, isTrue);
  });

  test('CliOptions parses combined flags with random position arguments', () {
    final options = CliOptions.fromArgs([
      'some_url',
      '--minimized',
      '--start',
      'another_arg',
    ]);
    expect(options.isMinimized, isTrue);
    expect(options.isStart, isTrue);
  });
}
