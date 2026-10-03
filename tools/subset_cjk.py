import argparse
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

parser = argparse.ArgumentParser()
parser.add_argument("source")
parser.add_argument("output")
args = parser.parse_args()
font = TTFont(args.source, fontNumber=2, recalcTimestamp=False)
options = subset.Options()
options.name_IDs = ["*"]
options.name_legacy = True
options.name_languages = ["*"]
subsetter = subset.Subsetter(options=options)
subsetter.populate(text=" 中文你好日本語按钮更新文本世界")
subsetter.subset(font)
font = instantiateVariableFont(font, {"wght": 400}, inplace=True, updateFontNames=True)
font.save(args.output)
