inotifywait -q -m -e close_write kilo.c |
while read -r filename event; do
	make;
	echo "Kilo Rebuilt at : `date`"
done
